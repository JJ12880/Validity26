// Copyright (c) 2024-2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_STAKINGPAGE_H
#define BITCOIN_QT_STAKINGPAGE_H

#include <QElapsedTimer>
#include <QString>
#include <QWidget>
#include <consensus/amount.h>

class ClientModel;
class GaugeWidget;
class PlatformStyle;
class WalletModel;

QT_BEGIN_NAMESPACE
class QHideEvent;
class QShowEvent;
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

public Q_SLOTS:
    //! VALIDITY: Settings > Mask values was toggled; redraw the amounts
    void setPrivacy(bool privacy);

protected:
    // VALIDITY: Timers and wallet queries only run while the page is shown.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    Ui::StakingPage* ui;
    ClientModel* clientModel{nullptr};
    WalletModel* walletModel{nullptr};
    const PlatformStyle* m_platform_style;
    bool m_privacy{false}; // VALIDITY: mask the wallet's own amounts (Settings > Mask values)
    bool privacy() const { return m_privacy; }
    QTimer* m_refresh_timer{nullptr};
    QTimer* m_stake_timer{nullptr};
    QTimer* m_anim_timer{nullptr};
    QTimer* m_report_timer{nullptr};     // VALIDITY: coalesces staking report refreshes
    QTimer* m_spinner_timeout{nullptr};  // VALIDITY: gives up on the spinner if staking never starts
    GaugeWidget* m_gaugeNetworkShare{nullptr};
    GaugeWidget* m_gaugeNetworkWeight{nullptr};
    GaugeWidget* m_gaugeAPR{nullptr};
    GaugeWidget* m_gaugeStakeTimer{nullptr};

    double m_expectedStakeTime{0};
    double m_stakeTimerMax{0};
    double m_stakeTimerElapsed{0};
    QElapsedTimer m_stakeTimerClock;     // VALIDITY: countdown survives the page being hidden

    // VALIDITY: Page visibility / refresh bookkeeping
    bool m_page_visible{false};
    bool m_anim_paused{false};
    bool m_report_dirty{true};
    int m_refresh_count{0};
    QElapsedTimer m_last_report;
    QString m_status_style;

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
    // VALIDITY: Give up waiting for staking to start after this long
    static constexpr int SPINNER_TIMEOUT_MS = 60 * 1000;
    // VALIDITY: Minimum spacing between balance-triggered staking report refreshes
    static constexpr int REPORT_MIN_INTERVAL_MS = 30 * 1000;

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
    void scheduleStakingReport();
    void resetStakeTimer(double expectedSeconds);
    void startGaugeSweepAnimation();
    void exitSpinnerMode();
    void setStakingStatusLabel(const QString& text, const QString& style);

private Q_SLOTS:
    void onBalanceChanged();
    void onEncryptionStatusChanged();
    void onTimerRefresh();
    void onStakeTimerTick();
    void onAnimationTick();
    void onSpinnerTimeout();
    void onUnlockForStakingClicked();
    void onStopStakingClicked();
    void onSetReserveClicked();
};

#endif // BITCOIN_QT_STAKINGPAGE_H
