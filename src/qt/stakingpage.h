// Copyright (c) 2024-2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_STAKINGPAGE_H
#define BITCOIN_QT_STAKINGPAGE_H

#include <QWidget>
#include <consensus/amount.h>

class ClientModel;
class PlatformStyle;
class WalletModel;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace Ui {
    class StakingPage;
}

class StakingPage : public QWidget
{
    Q_OBJECT

public:
    explicit StakingPage(const PlatformStyle* platformStyle, QWidget* parent = nullptr);
    ~StakingPage();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);

private:
    Ui::StakingPage* ui;
    ClientModel* clientModel{nullptr};
    WalletModel* walletModel{nullptr};
    const PlatformStyle* m_platform_style;
    QTimer* m_refresh_timer{nullptr};

    void updateStakingStatus();
    void updateStakingReport();

private Q_SLOTS:
    void onBalanceChanged();
    void onEncryptionStatusChanged();
    void onTimerRefresh();
    void onUnlockForStakingClicked();
};

#endif // BITCOIN_QT_STAKINGPAGE_H
