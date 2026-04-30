// Copyright (c) 2024-2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/stakingpage.h>
#include <qt/forms/ui_stakingpage.h>

#include <qt/askpassphrasedialog.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/gaugewidget.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionrecord.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <chainparams.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <util/moneystr.h>
#include <validation.h>

#include <QDateTime>
#include <QInputDialog>
#include <QTimer>
#include <QDebug>

// Truncate VAL amounts to 4 decimal places for cleaner display
static QString formatVal4(BitcoinUnit unit, CAmount amount)
{
    QString s = BitcoinUnits::formatWithUnit(unit, amount, false, BitcoinUnits::SeparatorStyle::ALWAYS);
    // Find the decimal point and truncate to 4 places after it
    int dot = s.indexOf('.');
    if (dot >= 0) {
        // Find where the digits end (before the unit suffix like " VAL")
        int spaceAfter = s.indexOf(' ', dot);
        if (spaceAfter < 0) spaceAfter = s.length();
        int decimals = spaceAfter - dot - 1;
        if (decimals > 4) {
            s = s.left(dot + 5) + s.mid(spaceAfter);
        }
    }
    return s;
}

StakingPage::StakingPage(const PlatformStyle* platformStyle, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::StakingPage)
    , m_platform_style(platformStyle)
{
    ui->setupUi(this);

    ui->btnUnlockStaking->setMaximumWidth(280);

    // Stake timer gauge (top row)
    m_gaugeStakeTimer = new GaugeWidget(this);
    m_gaugeStakeTimer->setLabel("Next Stake");
    m_gaugeStakeTimer->setLabelAbove(true);
    m_gaugeStakeTimer->setMaxValue(100.0);
    m_gaugeStakeTimer->setValueText("Not staking");
    m_gaugeStakeTimer->setSubtitle("*Estimate \xe2\x80\x94 times vary");
    ui->layoutStakeTimer->addWidget(m_gaugeStakeTimer);

    // Bottom gauge tiles
    m_gaugeNetworkShare = new GaugeWidget(this);
    m_gaugeNetworkShare->setLabel("Network Share");
    m_gaugeNetworkShare->setLabelAbove(true);
    m_gaugeNetworkShare->setMaxValue(100.0);
    m_gaugeNetworkShare->setValueText("Not staking");
    ui->layoutGaugeShare->addWidget(m_gaugeNetworkShare);

    m_gaugeNetworkWeight = new GaugeWidget(this);
    m_gaugeNetworkWeight->setLabel("Net Staking / Supply");
    m_gaugeNetworkWeight->setLabelAbove(true);
    m_gaugeNetworkWeight->setMaxValue(100.0);
    m_gaugeNetworkWeight->setValueText("0.00%");
    ui->layoutGaugeWeight->addWidget(m_gaugeNetworkWeight);

    m_gaugeAPR = new GaugeWidget(this);
    m_gaugeAPR->setLabel("Staking APR");
    m_gaugeAPR->setLabelAbove(true);
    m_gaugeAPR->setMaxValue(20.0);
    m_gaugeAPR->setValueText("0.00%");
    ui->layoutGaugeAPR->addWidget(m_gaugeAPR);

    // Equal stretch for all gauge tiles
    ui->layoutStatTiles->setStretch(0, 1);
    ui->layoutStatTiles->setStretch(1, 1);
    ui->layoutStatTiles->setStretch(2, 1);

    // Consistent tile spacing (match vertical spacing of 8px)
    ui->layoutStatusRow->setSpacing(8);
    ui->layoutStatTiles->setSpacing(8);

    // Top row: equal halves so separators align with middle row (2 of 3 tiles)
    ui->layoutStatusRow->setStretch(0, 2);
    ui->layoutStatusRow->setStretch(1, 1);

    // 1-second timer for stake countdown
    m_stake_timer = new QTimer(this);
    connect(m_stake_timer, &QTimer::timeout, this, &StakingPage::onStakeTimerTick);

    // Animation timer (50ms tick for smooth sweep)
    m_anim_timer = new QTimer(this);
    connect(m_anim_timer, &QTimer::timeout, this, &StakingPage::onAnimationTick);

    connect(ui->btnUnlockStaking, &QPushButton::clicked, this, &StakingPage::onUnlockForStakingClicked);
    connect(ui->btnStopStaking, &QPushButton::clicked, this, &StakingPage::onStopStakingClicked);
    connect(ui->btnSetReserve, &QPushButton::clicked, this, &StakingPage::onSetReserveClicked);
}

StakingPage::~StakingPage()
{
    delete ui;
}

void StakingPage::setClientModel(ClientModel* model)
{
    clientModel = model;
}

void StakingPage::setWalletModel(WalletModel* model)
{
    walletModel = model;
    if (!model) {
        if (m_refresh_timer) m_refresh_timer->stop();
        if (m_stake_timer) m_stake_timer->stop();
        if (m_anim_timer) m_anim_timer->stop();
        return;
    }

    connect(model, &WalletModel::balanceChanged, this, &StakingPage::onBalanceChanged);
    connect(model, &WalletModel::encryptionStatusChanged, this, &StakingPage::onEncryptionStatusChanged);

    m_refresh_timer = new QTimer(this);
    connect(m_refresh_timer, &QTimer::timeout, this, &StakingPage::onTimerRefresh);
    m_refresh_timer->start(10000);

    onEncryptionStatusChanged();
    updateStakingStatus();
    updateStakingReport();
}

void StakingPage::onBalanceChanged()
{
    resetStakeTimer(m_expectedStakeTime);
    updateStakingReport();
}

void StakingPage::onEncryptionStatusChanged()
{
    if (!walletModel) return;
    WalletModel::EncryptionStatus status = walletModel->getEncryptionStatus();
    ui->btnUnlockStaking->setVisible(status == WalletModel::Locked);
    ui->btnStopStaking->setVisible(status != WalletModel::Locked);

    // Update reserve balance display
    if (walletModel->getOptionsModel()) {
        QString reserveStr = walletModel->getOptionsModel()->data(
            walletModel->getOptionsModel()->index(OptionsModel::ReserveBalance, 0), Qt::EditRole).toString();
        CAmount reserveAmount = ParseMoney(reserveStr.toStdString()).value_or(0);
        BitcoinUnit unit = walletModel->getOptionsModel()->getDisplayUnit();
        ui->labelReserveBalance->setText(formatVal4(unit, reserveAmount));
    }
}

void StakingPage::onTimerRefresh()
{
    updateStakingStatus();
    static int refreshCount = 0;
    if (++refreshCount % 6 == 0) {
        updateStakingReport();
    }
}

void StakingPage::onStakeTimerTick()
{
    m_stakeTimerElapsed += 1.0;

    double fraction = (m_stakeTimerMax > 0) ? (m_stakeTimerElapsed / m_stakeTimerMax) : 0.0;
    m_gaugeStakeTimer->setValue(std::min(fraction * 100.0, 100.0));

    double remaining = m_expectedStakeTime - m_stakeTimerElapsed;
    if (remaining > 0) {
        int mins = (int)(remaining / 60.0);
        int secs = (int)(remaining) % 60;
        m_gaugeStakeTimer->setValueText(QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0')));
    } else {
        m_gaugeStakeTimer->setValueText(tr("Stake expected\nsoon!"));
    }
}

void StakingPage::resetStakeTimer(double expectedSeconds)
{
    m_expectedStakeTime = expectedSeconds;
    m_stakeTimerMax = expectedSeconds * 1.2;
    m_stakeTimerElapsed = 0;

    if (expectedSeconds > 0) {
        m_gaugeStakeTimer->setMaxValue(100.0);
        m_gaugeStakeTimer->setValue(0);
        int mins = (int)(expectedSeconds / 60.0);
        int secs = (int)(expectedSeconds) % 60;
        m_gaugeStakeTimer->setValueText(QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0')));
        if (!m_stake_timer->isActive()) {
            m_stake_timer->start(1000);
        }
    } else {
        m_stake_timer->stop();
        m_gaugeStakeTimer->setValue(0);
        m_gaugeStakeTimer->setValueText("Not staking");
    }
}

void StakingPage::startGaugeSweepAnimation()
{
    m_animStep = 0;
    m_animRanThisCycle = true;
    m_wasStaking = true;
    m_spinnerMode = false;

    // Show initializing text during animation
    m_gaugeNetworkShare->setValueText("");
    m_gaugeStakeTimer->setValueText("");
    m_gaugeAPR->setValueText("");

    m_anim_timer->start(50);
}

void StakingPage::onAnimationTick()
{
    if (m_spinnerMode) {
        // We're in fill-to-target mode after spinner stopped
        m_animStep++;

        if (m_animStep <= ANIM_FILL_TARGET) {
            double frac = (double)m_animStep / ANIM_FILL_TARGET;
            double eased = 1.0 - (1.0 - frac) * (1.0 - frac); // ease-out

            m_gaugeNetworkShare->setValue(m_targetShare * eased);
            m_gaugeStakeTimer->setValue(0);
            m_gaugeAPR->setMaxValue(m_targetAPRMax);
            m_gaugeAPR->setValue(m_targetAPR * eased);
        } else {
            // Done — snap to final
            m_anim_timer->stop();
            m_spinnerMode = false;

            m_gaugeNetworkShare->setMaxValue(100.0);
            m_gaugeNetworkShare->setValue(m_targetShare);
            m_gaugeNetworkShare->setValueText(m_targetShareText);

            m_gaugeAPR->setMaxValue(m_targetAPRMax);
            m_gaugeAPR->setValue(m_targetAPR);
            m_gaugeAPR->setValueText(m_targetAPRText);

            m_gaugeStakeTimer->setValue(0);
            // Kick off the stake timer if we have expected time
            if (m_expectedStakeTime > 0) {
                resetStakeTimer(m_expectedStakeTime);
            }
        }
        return;
    }

    // Initial sweep animation: all gauges together
    m_animStep++;

    int p1 = ANIM_SWEEP_UP;
    int p2 = p1 + ANIM_HOLD_TOP;
    int p3 = p2 + ANIM_SWEEP_DOWN;

    auto setAll = [&](double val) {
        m_gaugeNetworkShare->setMaxValue(100.0);
        m_gaugeNetworkShare->setValue(val);
        m_gaugeStakeTimer->setMaxValue(100.0);
        m_gaugeStakeTimer->setValue(val);
        m_gaugeAPR->setMaxValue(100.0);
        m_gaugeAPR->setValue(val);
    };

    if (m_animStep <= p1) {
        double frac = (double)m_animStep / ANIM_SWEEP_UP;
        double eased = 1.0 - (1.0 - frac) * (1.0 - frac);
        setAll(eased * 100.0);
    } else if (m_animStep <= p2) {
        // hold at 100
    } else if (m_animStep <= p3) {
        int step = m_animStep - p2;
        double frac = (double)step / ANIM_SWEEP_DOWN;
        double eased = frac * frac;
        setAll(100.0 * (1.0 - eased));
    } else {
        // Sweep done — enter spinner mode
        setAll(0);
        m_gaugeNetworkShare->setSpinning(true);
        m_gaugeStakeTimer->setSpinning(true);
        m_gaugeAPR->setSpinning(true);
        m_gaugeNetworkShare->setValueText("...");
        m_gaugeStakeTimer->setValueText("...");
        m_gaugeAPR->setValueText("...");
        m_spinnerMode = true;
        m_anim_timer->stop(); // Stop the animation timer — spinners self-animate via paint
        // updateStakingStatus() will detect spinnerMode and call stopSpinnersAndFill() when data arrives
    }
}

void StakingPage::updateStakingStatus()
{
    if (!walletModel) return;

    // If sweep animation is running (not spinner), don't touch gauges
    if (m_anim_timer->isActive() && !m_spinnerMode) {
        BitcoinUnit unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : BitcoinUnit::BTC;
        uint64_t nWeight = walletModel->getStakeWeight();
        ui->labelYourWeight->setText(formatVal4(unit, nWeight));
        return;
    }

    // If spinners are active, check if staking data has arrived
    if (m_spinnerMode) {
        uint64_t nWeight = walletModel->getStakeWeight();
        int64_t nInterval = walletModel->wallet().getLastCoinStakeSearchInterval();
        bool fStaking = walletModel->wallet().getEnabledStaking() && (nInterval != 0) && (nWeight != 0);

        BitcoinUnit unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : BitcoinUnit::BTC;
        ui->labelYourWeight->setText(formatVal4(unit, nWeight));

        if (fStaking) {
            ui->labelStakingActive->setText(tr("Active"));
            ui->labelStakingActive->setStyleSheet("color: #4ecca3; font-weight: bold; font-size: 16px;");

            // Data arrived! Cache targets, stop spinners, start fill animation
            double nNetworkWeight = 1.1429 * walletModel->node().getPoSKernelPS();
            if (nNetworkWeight > 0 && nWeight > 0) {
                m_targetShare = (double)nWeight / nNetworkWeight * 100.0;
                m_targetShareText = QString::number(m_targetShare, 'f', 2) + "%";
            }
            int nHeight = clientModel ? clientModel->getNumBlocks() : 0;
            if (nNetworkWeight > 0 && nHeight > 0) {
                int64_t nTargetSpacing = Params().GetConsensus().nTargetSpacing;
                double blocksPerYear = 365.25 * 24.0 * 3600.0 / nTargetSpacing;
                CAmount subsidy = GetBlockSubsidy(nHeight, Params().GetConsensus(), true);
                double annualRewards = blocksPerYear * (double)subsidy;
                m_targetAPR = (annualRewards / nNetworkWeight) * 100.0;
                m_targetAPRText = QString::number(m_targetAPR, 'f', 2) + "%";
                m_targetAPRMax = std::max(5.0, std::ceil(m_targetAPR / 5.0) * 5.0);
            }

            // Calculate expected time for timer
            if (nWeight > 0 && nNetworkWeight > 0) {
                int64_t nTargetSpacing = Params().GetConsensus().nTargetSpacing;
                m_expectedStakeTime = 1.0455 * nTargetSpacing * nNetworkWeight / nWeight;
            }

            // Stop spinners
            m_gaugeNetworkShare->setSpinning(false);
            m_gaugeStakeTimer->setSpinning(false);
            m_gaugeAPR->setSpinning(false);

            // Start fill-to-target animation
            m_animStep = 0;
            m_anim_timer->start(50);
        }
        return;
    }

    uint64_t nWeight = walletModel->getStakeWeight();
    int64_t nInterval = walletModel->wallet().getLastCoinStakeSearchInterval();
    bool fStaking = walletModel->wallet().getEnabledStaking() && (nInterval != 0) && (nWeight != 0);

    double nNetworkWeight = 1.1429 * walletModel->node().getPoSKernelPS();
    double nExpectedTime = 0;

    if (fStaking && nWeight > 0 && nNetworkWeight > 0) {
        int64_t nTargetSpacing = Params().GetConsensus().nTargetSpacing;
        nExpectedTime = 1.0455 * nTargetSpacing * nNetworkWeight / nWeight;
    }

    // Status label
    if (fStaking) {
        ui->labelStakingActive->setText(tr("Active"));
        ui->labelStakingActive->setStyleSheet("color: #4ecca3; font-weight: bold; font-size: 16px;");
    } else if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        ui->labelStakingActive->setText(tr("Not staking, wallet locked"));
        ui->labelStakingActive->setStyleSheet("color: #e8a84e; font-weight: bold; font-size: 16px;");
    } else {
        ui->labelStakingActive->setText(tr("Inactive"));
        ui->labelStakingActive->setStyleSheet("color: #7a7a96; font-weight: bold; font-size: 16px;");
    }

    BitcoinUnit unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : BitcoinUnit::BTC;
    ui->labelYourWeight->setText(formatVal4(unit, nWeight));

    // Track staking state — but NEVER trigger animation from here if one already ran
    m_wasStaking = fStaking;
    if (!fStaking) m_animRanThisCycle = false;

    // Stake timer
    if (fStaking && nExpectedTime > 0) {
        double diff = std::abs(nExpectedTime - m_expectedStakeTime);
        if (m_expectedStakeTime <= 0 || diff / m_expectedStakeTime > 0.10) {
            resetStakeTimer(nExpectedTime);
        }
    } else if (!fStaking) {
        resetStakeTimer(0);
    }

    // Calculate gauge values
    double sharePct = 0;
    QString shareText = "Not staking";
    if (fStaking && nNetworkWeight > 0 && nWeight > 0) {
        sharePct = (double)nWeight / nNetworkWeight * 100.0;
        shareText = QString::number(sharePct, 'f', 2) + "%";
    }

    static const double MAX_SUPPLY_SAT = 900000000000000.0;
    double netPct = 0;
    QString netPctText = "0.00%";
    if (nNetworkWeight > 0) {
        netPct = (nNetworkWeight / MAX_SUPPLY_SAT) * 100.0;
        netPctText = QString::number(netPct, 'f', 2) + "%";
        QString netWtStr = formatVal4(unit, (CAmount)nNetworkWeight);
        m_gaugeNetworkWeight->setSubtitle(netWtStr + " / 9M VAL");
    } else {
        m_gaugeNetworkWeight->setSubtitle("");
    }

    double apr = 0;
    double aprMax = 5.0;
    QString aprText = "Not staking";
    int nHeight = clientModel ? clientModel->getNumBlocks() : 0;
    if (fStaking && nNetworkWeight > 0 && nHeight > 0) {
        int64_t nTargetSpacing = Params().GetConsensus().nTargetSpacing;
        double blocksPerYear = 365.25 * 24.0 * 3600.0 / nTargetSpacing;
        CAmount subsidy = GetBlockSubsidy(nHeight, Params().GetConsensus(), true);
        double annualRewards = blocksPerYear * (double)subsidy;
        apr = (annualRewards / nNetworkWeight) * 100.0;
        aprText = QString::number(apr, 'f', 2) + "%";
        aprMax = std::max(5.0, std::ceil(apr / 5.0) * 5.0);
    }

    // Update all gauges directly (no animation running)
    m_gaugeNetworkWeight->setValue(netPct);
    m_gaugeNetworkWeight->setValueText(netPctText);
    m_gaugeNetworkShare->setValue(sharePct);
    m_gaugeNetworkShare->setValueText(shareText);
    m_gaugeAPR->setValue(apr);
    m_gaugeAPR->setValueText(aprText);
    m_gaugeAPR->setMaxValue(aprMax);
}

void StakingPage::updateStakingReport()
{
    if (!walletModel) return;

    TransactionTableModel* ttm = walletModel->getTransactionTableModel();
    if (!ttm) return;

    int rows = ttm->rowCount(QModelIndex());
    CAmount sum24h = 0, sum7d = 0, sum30d = 0, sum1y = 0, sumTotal = 0;
    qint64 now = QDateTime::currentDateTime().toSecsSinceEpoch();

    for (int i = 0; i < rows; ++i) {
        QModelIndex idx = ttm->index(i, 0);
        int type = ttm->data(idx, TransactionTableModel::TypeRole).toInt();
        if (type != TransactionRecord::Staked && type != TransactionRecord::Generated) continue;

        CAmount amt = ttm->data(idx, TransactionTableModel::AmountRole).toLongLong();
        if (amt <= 0) continue;

        qint64 txTime = ttm->data(idx, TransactionTableModel::DateRole).toDateTime().toSecsSinceEpoch();
        if (txTime <= 0) continue;

        sumTotal += amt;
        qint64 age = now - txTime;
        if (age <= 365 * 86400) sum1y += amt;
        if (age <= 30 * 86400)  sum30d += amt;
        if (age <= 7 * 86400)   sum7d += amt;
        if (age <= 86400)        sum24h += amt;
    }

    BitcoinUnit unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : BitcoinUnit::BTC;
    ui->labelStaking24h->setText(formatVal4(unit, sum24h));
    ui->labelStaking7d->setText(formatVal4(unit, sum7d));
    ui->labelStaking30d->setText(formatVal4(unit, sum30d));
    ui->labelStaking1y->setText(formatVal4(unit, sum1y));
    ui->labelStakingTotal->setText(formatVal4(unit, sumTotal));
}

void StakingPage::onStopStakingClicked()
{
    if (!walletModel) return;
    walletModel->setWalletLocked(true);
    onEncryptionStatusChanged();
    updateStakingStatus();
}

void StakingPage::onSetReserveClicked()
{
    if (!walletModel || !walletModel->getOptionsModel()) return;

    // Get current reserve
    QString currentStr = walletModel->getOptionsModel()->data(
        walletModel->getOptionsModel()->index(OptionsModel::ReserveBalance, 0), Qt::EditRole).toString();
    CAmount currentAmount = ParseMoney(currentStr.toStdString()).value_or(0);

    bool ok;
    double currentVal = (double)currentAmount / 100000000.0;
    double newVal = QInputDialog::getDouble(this, tr("Set Reserve Balance"),
        tr("Amount of VAL to keep reserved (not used for staking):"),
        currentVal, 0, 9000000, 8, &ok);

    if (ok) {
        CAmount newAmount = (CAmount)(newVal * 100000000.0);
        QString newStr = QString::fromStdString(FormatMoney(newAmount));
        walletModel->getOptionsModel()->setData(
            walletModel->getOptionsModel()->index(OptionsModel::ReserveBalance, 0), newStr);
        onEncryptionStatusChanged(); // refresh display
    }
}

void StakingPage::onUnlockForStakingClicked()
{
    if (!walletModel) return;
    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        AskPassphraseDialog dlg(AskPassphraseDialog::UnlockStaking, this);
        dlg.setModel(walletModel);
        dlg.exec();
        // If unlock succeeded, fire the sweep animation immediately
        if (walletModel->getEncryptionStatus() != WalletModel::Locked) {
            // Pre-cache target values for the gauge row animation
            double nNetworkWeight = 1.1429 * walletModel->node().getPoSKernelPS();
            uint64_t nWeight = walletModel->getStakeWeight();

            if (nNetworkWeight > 0 && nWeight > 0) {
                m_targetShare = (double)nWeight / nNetworkWeight * 100.0;
                m_targetShareText = QString::number(m_targetShare, 'f', 2) + "%";
            } else {
                m_targetShare = 0;
                m_targetShareText = "0.00%";
            }

            int nHeight = clientModel ? clientModel->getNumBlocks() : 0;
            if (nNetworkWeight > 0 && nHeight > 0) {
                int64_t nTargetSpacing = Params().GetConsensus().nTargetSpacing;
                double blocksPerYear = 365.25 * 24.0 * 3600.0 / nTargetSpacing;
                CAmount subsidy = GetBlockSubsidy(nHeight, Params().GetConsensus(), true);
                m_targetAPR = (blocksPerYear * (double)subsidy / nNetworkWeight) * 100.0;
                m_targetAPRText = QString::number(m_targetAPR, 'f', 2) + "%";
                m_targetAPRMax = std::max(5.0, std::ceil(m_targetAPR / 5.0) * 5.0);
            } else {
                m_targetAPR = 0;
                m_targetAPRText = "Not staking";
                m_targetAPRMax = 5.0;
            }

            startGaugeSweepAnimation();
        }
    }
}
