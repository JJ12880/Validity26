// Copyright (c) 2024-2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/stakingpage.h>
#include <qt/forms/ui_stakingpage.h>

#include <qt/addressbookpage.h>
#include <qt/askpassphrasedialog.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/gaugewidget.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>

#include <chainparams.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <util/moneystr.h>
#include <util/strencodings.h>
#include <util/translation.h>
#include <validation.h>
#include <wallet/types.h>
#include <wallet/wallet.h>

#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QTimer>
#include <QDebug>
#include <QHideEvent>
#include <QShowEvent>

#include <algorithm>
#include <cmath>

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

// VALIDITY: an L2 address stake tag is 'S' + the address's base58 version byte + its hash160
// (22 bytes), the binary form the SmartChain L2 uses for an L1 address.
static constexpr unsigned char STAKE_TAG_L2_MARKER = 0x53;

static std::string L2AddressToTagHex(const std::string& address)
{
    const CTxDestination dest = DecodeDestination(address);
    std::vector<unsigned char> tag{STAKE_TAG_L2_MARKER};
    if (const auto* pkh = std::get_if<PKHash>(&dest)) {
        tag.push_back(Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS).at(0));
        tag.insert(tag.end(), pkh->begin(), pkh->end());
    } else if (const auto* sh = std::get_if<ScriptHash>(&dest)) {
        tag.push_back(Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS).at(0));
        tag.insert(tag.end(), sh->begin(), sh->end());
    } else {
        return {};
    }
    return HexStr(tag);
}

//! The address an L2 address tag encodes, or empty if the tag is something else.
static std::string TagHexToL2Address(const std::string& hex)
{
    const std::vector<unsigned char> tag = ParseHex(hex);
    if (tag.size() != 2 + uint160::size() || tag[0] != STAKE_TAG_L2_MARKER) return {};
    uint160 hash;
    std::copy(tag.begin() + 2, tag.end(), hash.begin());
    if (tag[1] == Params().Base58Prefix(CChainParams::PUBKEY_ADDRESS).at(0)) return EncodeDestination(PKHash(hash));
    if (tag[1] == Params().Base58Prefix(CChainParams::SCRIPT_ADDRESS).at(0)) return EncodeDestination(ScriptHash(hash));
    return {};
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

    // VALIDITY: Periodic status refresh (started/stopped with page visibility)
    m_refresh_timer = new QTimer(this);
    connect(m_refresh_timer, &QTimer::timeout, this, &StakingPage::onTimerRefresh);

    // VALIDITY: Single-shot timer that coalesces staking report refreshes
    m_report_timer = new QTimer(this);
    m_report_timer->setSingleShot(true);
    connect(m_report_timer, &QTimer::timeout, this, &StakingPage::updateStakingReport);

    // VALIDITY: Stop the gauge spinners if staking does not start after unlocking
    m_spinner_timeout = new QTimer(this);
    m_spinner_timeout->setSingleShot(true);
    connect(m_spinner_timeout, &QTimer::timeout, this, &StakingPage::onSpinnerTimeout);

    connect(ui->btnUnlockStaking, &QPushButton::clicked, this, &StakingPage::onUnlockForStakingClicked);
    connect(ui->btnStopStaking, &QPushButton::clicked, this, &StakingPage::onStopStakingClicked);
    connect(ui->btnSetReserve, &QPushButton::clicked, this, &StakingPage::onSetReserveClicked);
    connect(ui->btnSetDonation, &QPushButton::clicked, this, &StakingPage::onSetDonationClicked);
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
        m_refresh_timer->stop();
        m_stake_timer->stop();
        m_anim_timer->stop();
        m_report_timer->stop();
        m_spinner_timeout->stop();
        return;
    }

    connect(model, &WalletModel::balanceChanged, this, &StakingPage::onBalanceChanged);
    connect(model, &WalletModel::encryptionStatusChanged, this, &StakingPage::onEncryptionStatusChanged);

    m_report_dirty = true;
    onEncryptionStatusChanged();

    // VALIDITY: The rest is deferred to showEvent() if the page is not visible yet.
    if (m_page_visible) {
        m_refresh_timer->start(10000);
        updateStakingStatus();
        updateStakingReport();
    }
}

void StakingPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_page_visible = true;
    if (!walletModel) return;

    m_refresh_timer->start(10000);
    if (m_anim_paused) {
        m_anim_paused = false;
        m_anim_timer->start(50);
    }
    updateStakingStatus();
    // Resume the countdown (elapsed time is wall-clock based, so it stays accurate)
    if (m_expectedStakeTime > 0 && !m_spinnerMode && !m_anim_timer->isActive() && !m_stake_timer->isActive()) {
        m_stake_timer->start(1000);
        onStakeTimerTick();
    }
    if (m_report_dirty) {
        updateStakingReport();
    }
}

void StakingPage::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // VALIDITY: Nothing on this page needs updating while it is not shown.
    m_page_visible = false;
    m_refresh_timer->stop();
    m_stake_timer->stop();
    m_report_timer->stop();
    if (m_anim_timer->isActive()) {
        m_anim_timer->stop();
        m_anim_paused = true;
    }
}

void StakingPage::onBalanceChanged()
{
    // Don't disturb the unlock sweep / spinner animation
    if (!m_spinnerMode && !m_anim_timer->isActive() && !m_anim_paused) {
        resetStakeTimer(m_expectedStakeTime);
    }
    scheduleStakingReport();
}

void StakingPage::scheduleStakingReport()
{
    // VALIDITY: balanceChanged can fire every block; coalesce report refreshes and space
    // them at least REPORT_MIN_INTERVAL_MS apart. While hidden, just mark dirty.
    m_report_dirty = true;
    if (!m_page_visible || m_report_timer->isActive()) return;
    int delay = 2000;
    if (m_last_report.isValid()) {
        delay = static_cast<int>(std::max<qint64>(delay, REPORT_MIN_INTERVAL_MS - m_last_report.elapsed()));
    }
    m_report_timer->start(delay);
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
    updateDonationLabel();

    // VALIDITY: If the wallet was relocked while the gauges wait for staking to start,
    // leave spinner mode now so "Not staking, wallet locked" is shown.
    if (status == WalletModel::Locked && (m_spinnerMode || m_anim_timer->isActive() || m_anim_paused)) {
        exitSpinnerMode();
        if (m_page_visible) updateStakingStatus();
    }
}

void StakingPage::onTimerRefresh()
{
    updateStakingStatus();
    // VALIDITY: per-instance counter (was a function static shared by all wallets)
    if (++m_refresh_count % 6 == 0) {
        updateStakingReport();
    }
}

void StakingPage::onStakeTimerTick()
{
    // VALIDITY: wall-clock elapsed time, so pausing the tick timer while hidden is harmless
    m_stakeTimerElapsed = m_stakeTimerClock.isValid() ? m_stakeTimerClock.elapsed() / 1000.0 : m_stakeTimerElapsed + 1.0;

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
    m_stakeTimerClock.start();

    if (expectedSeconds > 0) {
        m_gaugeStakeTimer->setMaxValue(100.0);
        m_gaugeStakeTimer->setValue(0);
        int mins = (int)(expectedSeconds / 60.0);
        int secs = (int)(expectedSeconds) % 60;
        m_gaugeStakeTimer->setValueText(QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0')));
        // VALIDITY: the 1s tick only runs while the page is shown (showEvent resumes it)
        if (m_page_visible && !m_stake_timer->isActive()) {
            m_stake_timer->start(1000);
        }
    } else {
        m_stakeTimerClock.invalidate();
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
        // updateStakingStatus() will detect spinnerMode and start the fill animation when data arrives
        // VALIDITY: ...or leave spinner mode if staking cannot start / does not start in time
        m_spinner_timeout->start(SPINNER_TIMEOUT_MS);
        if (m_page_visible) updateStakingStatus();
    }
}

void StakingPage::exitSpinnerMode()
{
    // VALIDITY: Abort the unlock sweep / spinner / fill animation; the next
    // updateStakingStatus() call shows the normal (non-animated) status.
    m_anim_timer->stop();
    m_anim_paused = false;
    m_spinner_timeout->stop();
    m_spinnerMode = false;
    m_gaugeNetworkShare->setSpinning(false);
    m_gaugeStakeTimer->setSpinning(false);
    m_gaugeAPR->setSpinning(false);
    m_gaugeNetworkShare->setMaxValue(100.0);
    m_gaugeStakeTimer->setMaxValue(100.0);
}

void StakingPage::onSpinnerTimeout()
{
    if (!m_spinnerMode || m_anim_timer->isActive() || m_anim_paused) return; // not waiting (anymore)
    exitSpinnerMode();
    if (m_page_visible) updateStakingStatus();
}

void StakingPage::setStakingStatusLabel(const QString& text, const QString& style)
{
    // VALIDITY: Only touch the label when something changed (setStyleSheet forces a re-polish)
    if (ui->labelStakingActive->text() != text) {
        ui->labelStakingActive->setText(text);
    }
    if (m_status_style != style) {
        m_status_style = style;
        ui->labelStakingActive->setStyleSheet(style);
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

    // VALIDITY: Leave spinner mode if staking cannot start (wallet relocked or staking
    // disabled); the normal status path below then reports why.
    if (m_spinnerMode && (walletModel->getEncryptionStatus() == WalletModel::Locked ||
                          !walletModel->wallet().getEnabledStaking())) {
        exitSpinnerMode();
    }

    // If spinners are active, check if staking data has arrived
    if (m_spinnerMode) {
        uint64_t nWeight = walletModel->getStakeWeight();
        int64_t nInterval = walletModel->wallet().getLastCoinStakeSearchInterval();
        bool fStaking = walletModel->wallet().getEnabledStaking() && (nInterval != 0) && (nWeight != 0);

        BitcoinUnit unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : BitcoinUnit::BTC;
        ui->labelYourWeight->setText(formatVal4(unit, nWeight));

        // VALIDITY: Fill-to-target animation already running; don't restart it.
        if (m_anim_timer->isActive()) return;

        if (fStaking) {
            m_spinner_timeout->stop();
            setStakingStatusLabel(tr("Active"), "color: #4ecca3; font-weight: bold; font-size: 16px;");

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
        } else {
            // VALIDITY: Unlocked but not staking yet; don't keep showing "wallet locked".
            setStakingStatusLabel(tr("Inactive"), "color: #7a7a96; font-weight: bold; font-size: 16px;");
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
        setStakingStatusLabel(tr("Active"), "color: #4ecca3; font-weight: bold; font-size: 16px;");
    } else if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        setStakingStatusLabel(tr("Not staking, wallet locked"), "color: #e8a84e; font-weight: bold; font-size: 16px;");
    } else {
        setStakingStatusLabel(tr("Inactive"), "color: #7a7a96; font-weight: bold; font-size: 16px;");
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

    // VALIDITY: Not shown, so don't compute; showEvent() refreshes if dirty.
    if (!m_page_visible) {
        m_report_dirty = true;
        return;
    }

    // VALIDITY: Computed by the wallet in a single pass over its transactions instead of
    // walking every TransactionTableModel row on the GUI thread (each row took wallet and
    // cs_main locks). Orphaned/abandoned/conflicted stakes are excluded.
    interfaces::WalletStakeReport report;
    const qint64 now = QDateTime::currentDateTime().toSecsSinceEpoch();
    if (!walletModel->wallet().tryGetStakeReport(report, now)) {
        // Wallet busy (e.g. rescan or staking); try again shortly.
        m_report_dirty = true;
        if (!m_report_timer->isActive()) m_report_timer->start(2000);
        return;
    }
    m_report_dirty = false;
    m_last_report.start();

    BitcoinUnit unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : BitcoinUnit::BTC;
    ui->labelStaking24h->setText(formatVal4(unit, report.last_24h));
    ui->labelStaking7d->setText(formatVal4(unit, report.last_7d));
    ui->labelStaking30d->setText(formatVal4(unit, report.last_30d));
    ui->labelStaking1y->setText(formatVal4(unit, report.last_1y));
    ui->labelStakingTotal->setText(formatVal4(unit, report.total));
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

void StakingPage::updateDonationLabel()
{
    if (!walletModel) return;
    const unsigned int percent = walletModel->wallet().getDonationPercentage();
    const QString address = QString::fromStdString(walletModel->wallet().getDonationAddress());
    auto shorten = [](const QString& s) { return s.length() > 20 ? s.left(10) + QStringLiteral("…") + s.right(6) : s; };
    auto yours = [&](const QString& addr) {
        wallet::isminetype is_mine = wallet::ISMINE_NO;
        walletModel->wallet().getAddress(DecodeDestination(addr.toStdString()), /*name=*/nullptr, &is_mine, /*purpose=*/nullptr);
        return is_mine != wallet::ISMINE_NO ? QStringLiteral(" ") + tr("(yours)") : QString();
    };

    if (percent == 0 || address.isEmpty()) {
        ui->labelStakeDonation->setText(tr("None"));
        ui->labelStakeDonation->setToolTip(QString());
    } else {
        ui->labelStakeDonation->setText(tr("%1% of rewards → %2").arg(percent).arg(shorten(address)) + yours(address));
        ui->labelStakeDonation->setToolTip(address);
    }

    const std::string tagHex = walletModel->wallet().getStakeTag();
    const QString tagAddress = QString::fromStdString(TagHexToL2Address(tagHex));
    if (tagHex.empty()) {
        ui->labelStakeTag->setText(tr("None"));
        ui->labelStakeTag->setToolTip(QString());
    } else if (!tagAddress.isEmpty()) {
        ui->labelStakeTag->setText(tr("L2 address %1").arg(shorten(tagAddress)) + yours(tagAddress));
        ui->labelStakeTag->setToolTip(tagAddress);
    } else {
        const QString hex = QString::fromStdString(tagHex);
        ui->labelStakeTag->setText(tr("%1 (%n byte(s))", "", hex.length() / 2).arg(shorten(hex)));
        ui->labelStakeTag->setToolTip(hex);
    }
}

void StakingPage::onSetDonationClicked()
{
    if (!walletModel || !walletModel->getOptionsModel()) return;

    // VALIDITY-PORT: the donation and stake tag are applied to the running wallet at once (the
    // staker picks them up for its next coinstake) and saved to settings.json for the next start.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Stake donation and tag"));

    auto bookButtonFor = [&](QLineEdit* edit) {
        auto* button = new QToolButton(&dlg);
        button->setIcon(m_platform_style->SingleColorIcon(":/icons/address-book"));
        button->setToolTip(tr("Choose from the address book"));
        connect(button, &QToolButton::clicked, &dlg, [&, edit] {
            AddressBookPage book(m_platform_style, AddressBookPage::ForSelection, AddressBookPage::SendingTab, &dlg);
            book.setModel(walletModel->getAddressTableModel());
            if (book.exec()) edit->setText(book.getReturnValue());
        });
        return button;
    };

    // Donation
    auto* donationBox = new QGroupBox(tr("Donation"), &dlg);
    auto* spin = new QSpinBox(donationBox);
    spin->setRange(1, wallet::MAX_DONATION_PERCENTAGE);
    spin->setSuffix(QStringLiteral("%"));
    auto* addressEdit = new QLineEdit(donationBox);
    addressEdit->setPlaceholderText(tr("X... or Q... address (empty for no donation)"));
    addressEdit->setMinimumWidth(320);

    const unsigned int currentPercent = walletModel->wallet().getDonationPercentage();
    spin->setValue(currentPercent > 0 ? int(currentPercent) : 10);
    addressEdit->setText(QString::fromStdString(walletModel->wallet().getDonationAddress()));

    auto* donationRow1 = new QHBoxLayout;
    donationRow1->addWidget(new QLabel(tr("Donate"), donationBox));
    donationRow1->addWidget(spin);
    donationRow1->addWidget(new QLabel(tr("of each stake reward"), donationBox));
    donationRow1->addStretch();
    auto* donationRow2 = new QHBoxLayout;
    donationRow2->addWidget(new QLabel(tr("to"), donationBox));
    donationRow2->addWidget(addressEdit, 1);
    donationRow2->addWidget(bookButtonFor(addressEdit));
    auto* donationNote = new QLabel(tr("Paid from the reward only, never your staked coins. "
                                       "Skipped on dev fund blocks and when below 0.01 VAL."), donationBox);
    donationNote->setWordWrap(true);
    auto* donationLayout = new QVBoxLayout(donationBox);
    donationLayout->addLayout(donationRow1);
    donationLayout->addLayout(donationRow2);
    donationLayout->addWidget(donationNote);

    // Stake tag
    auto* tagBox = new QGroupBox(tr("Stake tag"), &dlg);
    auto* radioL2 = new QRadioButton(tr("L2 address"), tagBox);
    auto* radioHex = new QRadioButton(tr("Raw hex"), tagBox);
    auto* tagAddressEdit = new QLineEdit(tagBox);
    tagAddressEdit->setPlaceholderText(tr("X... or Q... address"));
    auto* tagHexEdit = new QLineEdit(tagBox);
    tagHexEdit->setPlaceholderText(tr("up to %1 bytes").arg(wallet::MAX_STAKE_TAG_BYTES));
    tagHexEdit->setMaxLength(int(wallet::MAX_STAKE_TAG_BYTES) * 2);
    tagHexEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9a-fA-F]*")), tagHexEdit));
    auto* tagBook = bookButtonFor(tagAddressEdit);

    const std::string currentTag = walletModel->wallet().getStakeTag();
    const std::string currentTagAddress = TagHexToL2Address(currentTag);
    if (currentTag.empty() || !currentTagAddress.empty()) {
        radioL2->setChecked(true);
        tagAddressEdit->setText(QString::fromStdString(currentTagAddress));
    } else {
        radioHex->setChecked(true);
        tagHexEdit->setText(QString::fromStdString(currentTag));
    }
    auto syncTagMode = [=] {
        tagAddressEdit->setEnabled(radioL2->isChecked());
        tagBook->setEnabled(radioL2->isChecked());
        tagHexEdit->setEnabled(radioHex->isChecked());
    };
    syncTagMode();
    connect(radioL2, &QRadioButton::toggled, &dlg, syncTagMode);

    auto* tagGrid = new QGridLayout;
    tagGrid->addWidget(radioL2, 0, 0);
    tagGrid->addWidget(tagAddressEdit, 0, 1);
    tagGrid->addWidget(tagBook, 0, 2);
    tagGrid->addWidget(radioHex, 1, 0);
    tagGrid->addWidget(tagHexEdit, 1, 1, 1, 2);
    auto* tagNote = new QLabel(tr("Published with each block you stake, in the output that carries the "
                                  "block-signing key. Public and permanent. Skipped on dev fund blocks."), tagBox);
    tagNote->setWordWrap(true);
    auto* tagLayout = new QVBoxLayout(tagBox);
    tagLayout->addLayout(tagGrid);
    tagLayout->addWidget(tagNote);

    auto* buttons = new QDialogButtonBox(&dlg);
    QPushButton* removeTagButton = buttons->addButton(tr("Remove tag"), QDialogButtonBox::DestructiveRole);
    removeTagButton->setEnabled(!currentTag.empty());
    QPushButton* removeDonationButton = buttons->addButton(tr("Remove donation"), QDialogButtonBox::DestructiveRole);
    removeDonationButton->setEnabled(currentPercent > 0);
    buttons->addButton(QDialogButtonBox::Cancel);
    QPushButton* applyButton = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    applyButton->setDefault(true);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(donationBox);
    layout->addWidget(tagBox);
    layout->addWidget(buttons);

    OptionsModel* options = walletModel->getOptionsModel();
    auto saveDonation = [&](unsigned int percent, const QString& address) {
        options->setData(options->index(OptionsModel::DonationPercentage, 0), qlonglong(percent));
        options->setData(options->index(OptionsModel::DonationAddress, 0), address);
    };
    auto saveTag = [&](const std::string& hex) {
        options->setData(options->index(OptionsModel::StakeTag, 0), QString::fromStdString(hex));
    };
    connect(removeDonationButton, &QPushButton::clicked, &dlg, [&] {
        bilingual_str error;
        walletModel->wallet().setStakeDonation(0, "", error);
        saveDonation(0, QString());
        dlg.accept();
    });
    connect(removeTagButton, &QPushButton::clicked, &dlg, [&] {
        bilingual_str error;
        walletModel->wallet().setStakeTag("", error);
        saveTag("");
        dlg.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, [&] {
        // Work out the tag first so a bad tag changes nothing.
        std::string tagHex;
        if (radioL2->isChecked()) {
            const QString tagAddress = tagAddressEdit->text().trimmed();
            if (!tagAddress.isEmpty()) {
                tagHex = L2AddressToTagHex(tagAddress.toStdString());
                if (tagHex.empty()) {
                    QMessageBox::warning(&dlg, tr("Stake tag"), tr("%1 is not a valid P2PKH or P2SH address.").arg(tagAddress));
                    return;
                }
            }
        } else {
            tagHex = tagHexEdit->text().trimmed().toLower().toStdString();
            if (tagHex.size() % 2 != 0) {
                QMessageBox::warning(&dlg, tr("Stake tag"), tr("The stake tag must be an even number of hex digits."));
                return;
            }
        }

        const QString address = addressEdit->text().trimmed();
        const unsigned int percent = address.isEmpty() ? 0 : spin->value();
        bilingual_str error;
        if (!walletModel->wallet().setStakeDonation(percent, address.toStdString(), error)) {
            QMessageBox::warning(&dlg, tr("Stake donation"), QString::fromStdString(error.translated));
            return; // keep the dialog open so the address can be corrected
        }
        saveDonation(percent, address);
        if (!walletModel->wallet().setStakeTag(tagHex, error)) {
            QMessageBox::warning(&dlg, tr("Stake tag"), QString::fromStdString(error.translated));
            return;
        }
        saveTag(tagHex);
        dlg.accept();
    });

    dlg.exec();
    updateDonationLabel();
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
