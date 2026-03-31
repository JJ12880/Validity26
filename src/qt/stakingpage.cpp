// Copyright (c) 2024-2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/stakingpage.h>
#include <qt/forms/ui_stakingpage.h>

#include <qt/askpassphrasedialog.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionrecord.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <chainparams.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>

#include <QDateTime>
#include <QTimer>

StakingPage::StakingPage(const PlatformStyle* platformStyle, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::StakingPage)
    , m_platform_style(platformStyle)
{
    ui->setupUi(this);
    connect(ui->btnUnlockStaking, &QPushButton::clicked, this, &StakingPage::onUnlockForStakingClicked);
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
        if (m_refresh_timer) {
            m_refresh_timer->stop();
        }
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
    updateStakingReport();
}

void StakingPage::onEncryptionStatusChanged()
{
    if (!walletModel) return;
    WalletModel::EncryptionStatus status = walletModel->getEncryptionStatus();
    ui->btnUnlockStaking->setVisible(status == WalletModel::Locked);
}

void StakingPage::onTimerRefresh()
{
    updateStakingStatus();
}

void StakingPage::updateStakingStatus()
{
    if (!walletModel) return;

    uint64_t nWeight = walletModel->getStakeWeight();
    int64_t nInterval = walletModel->wallet().getLastCoinStakeSearchInterval();
    bool fStaking = walletModel->wallet().getEnabledStaking() && (nInterval != 0) && (nWeight != 0);

    double nNetworkWeight = 0;
    double nExpectedTime = 0;
    if (fStaking) {
        nNetworkWeight = 1.1429 * walletModel->node().getPoSKernelPS();
        int64_t nTargetSpacing = Params().GetConsensus().nTargetSpacing;
        if (nWeight > 0 && nNetworkWeight > 0) {
            nExpectedTime = 1.0455 * nTargetSpacing * nNetworkWeight / nWeight;
        }
    }

    if (fStaking) {
        ui->labelStakingActive->setText(tr("Active"));
        ui->labelStakingActive->setStyleSheet("color: #4ecca3; font-weight: bold;");
    } else if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        ui->labelStakingActive->setText(tr("Locked"));
        ui->labelStakingActive->setStyleSheet("color: #e8a84e; font-weight: bold;");
    } else {
        ui->labelStakingActive->setText(tr("Inactive"));
        ui->labelStakingActive->setStyleSheet("color: #7a7a96; font-weight: bold;");
    }

    BitcoinUnit unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : BitcoinUnit::BTC;
    ui->labelYourWeight->setText(BitcoinUnits::formatWithUnit(unit, nWeight, false, BitcoinUnits::SeparatorStyle::ALWAYS));
    ui->labelNetworkWeight->setText(BitcoinUnits::formatWithUnit(unit, (CAmount)nNetworkWeight, false, BitcoinUnits::SeparatorStyle::ALWAYS));

    if (nExpectedTime > 0) {
        ui->labelExpectedTime->setText(GUIUtil::formatNiceTimeOffset(nExpectedTime));
    } else {
        ui->labelExpectedTime->setText(QString::fromUtf8("\xe2\x80\x94"));
    }

    if (nNetworkWeight > 0 && nWeight > 0) {
        double pct = (double)nWeight / nNetworkWeight * 100.0;
        ui->labelNetworkPercent->setText(QString::number(pct, 'f', 4) + "%");
        ui->labelNetworkShare->setText(QString::number(pct, 'f', 2) + "%");
    } else {
        ui->labelNetworkPercent->setText("0.00%");
        ui->labelNetworkShare->setText("0.00%");
    }

    if (walletModel->getOptionsModel()) {
        CAmount balance = walletModel->getCachedBalance().balance;
        ui->labelStakingCoins->setText(BitcoinUnits::formatWithUnit(unit, balance, false, BitcoinUnits::SeparatorStyle::ALWAYS));
    }
}

void StakingPage::updateStakingReport()
{
    if (!walletModel) return;

    TransactionTableModel* ttm = walletModel->getTransactionTableModel();
    if (!ttm) return;

    int rows = ttm->rowCount(QModelIndex());
    CAmount sum24h = 0, sum7d = 0, sum30d = 0, sum1y = 0, sumTotal = 0;
    qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();

    for (int i = 0; i < rows; ++i) {
        QModelIndex idx = ttm->index(i, 0);
        int type = ttm->data(idx, TransactionTableModel::TypeRole).toInt();
        if (type != TransactionRecord::Staked) continue;

        CAmount amt = ttm->data(idx, TransactionTableModel::AmountRole).toLongLong();
        if (amt <= 0) continue;

        qint64 txTime = ttm->data(idx, TransactionTableModel::DateRole).toDateTime().toSecsSinceEpoch();
        sumTotal += amt;
        if (now - txTime <= 365 * 86400) sum1y += amt;
        if (now - txTime <= 30 * 86400)  sum30d += amt;
        if (now - txTime <= 7 * 86400)   sum7d += amt;
        if (now - txTime <= 86400)        sum24h += amt;
    }

    BitcoinUnit unit = walletModel->getOptionsModel() ? walletModel->getOptionsModel()->getDisplayUnit() : BitcoinUnit::BTC;
    ui->labelStaking24h->setText(BitcoinUnits::formatWithUnit(unit, sum24h, false, BitcoinUnits::SeparatorStyle::ALWAYS));
    ui->labelStaking7d->setText(BitcoinUnits::formatWithUnit(unit, sum7d, false, BitcoinUnits::SeparatorStyle::ALWAYS));
    ui->labelStaking30d->setText(BitcoinUnits::formatWithUnit(unit, sum30d, false, BitcoinUnits::SeparatorStyle::ALWAYS));
    ui->labelStaking1y->setText(BitcoinUnits::formatWithUnit(unit, sum1y, false, BitcoinUnits::SeparatorStyle::ALWAYS));
    ui->labelStakingTotal->setText(BitcoinUnits::formatWithUnit(unit, sumTotal, false, BitcoinUnits::SeparatorStyle::ALWAYS));
}

void StakingPage::onUnlockForStakingClicked()
{
    if (!walletModel) return;
    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        AskPassphraseDialog dlg(AskPassphraseDialog::UnlockStaking, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}
