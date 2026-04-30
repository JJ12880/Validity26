// Copyright (c) 2024-2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_STAKINGPAGE_H
#define BITCOIN_QT_STAKINGPAGE_H

#include <QWidget>
#include <consensus/amount.h>

class ClientModel;
class GaugeWidget;
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
    QTimer* m_stake_timer{nullptr};
    QTimer* m_anim_timer{nullptr};
    GaugeWidget* m_gaugeNetworkShare{nullptr};
    GaugeWidget* m_gaugeNetworkWeight{nullptr};
    GaugeWidget* m_gaugeAPR{nullptr};
    GaugeWidget* m_gaugeStakeTimer{nullptr};

    double m_expectedStakeTime{0};
    double m_stakeTimerMax{0};
    double m_stakeTimerElapsed{0};

    // Animation state
    bool m_wasStaking{false};
    bool m_animRanThisCycle{false};
    bool m_spinnerMode{false};       // gauges are spinning waiting for data
    int m_animStep{0};
    // Sweep animation: fill→hold→empty then spinner
    static constexpr int ANIM_SWEEP_UP = 25;     // ~1.25s fill to 100%
    static constexpr int ANIM_HOLD_TOP = 6;      // ~0.3s hold at 100%
    static constexpr int ANIM_SWEEP_DOWN = 20;   // ~1.0s empty to 0%
    // Fill-to-target animation after spinner stops
    static constexpr int ANIM_FILL_TARGET = 25;  // ~1.25s fill to target

    // Cached target values for post-animation
    double m_targetShare{0};
    double m_targetNetPct{0};
    double m_targetAPR{0};
    double m_targetAPRMax{5.0};
    QString m_targetShareText;
    QString m_targetNetPctText;
    QString m_targetAPRText;

    void updateStakingStatus();
    void updateStakingReport();
    void resetStakeTimer(double expectedSeconds);
    void startGaugeSweepAnimation();

private Q_SLOTS:
    void onBalanceChanged();
    void onEncryptionStatusChanged();
    void onTimerRefresh();
    void onStakeTimerTick();
    void onAnimationTick();
    void onUnlockForStakingClicked();
    void onStopStakingClicked();
    void onSetReserveClicked();
};

#endif // BITCOIN_QT_STAKINGPAGE_H
