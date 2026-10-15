// Copyright (c) 2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MULTISIGDIALOG_H
#define BITCOIN_QT_MULTISIGDIALOG_H

#include <QDialog>
#include <QList>

class BitcoinAmountField;
class ClientModel;
class PlatformStyle;
class WalletModel;

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QVBoxLayout;
QT_END_NAMESPACE

/** VALIDITY: create a P2SH m-of-n multisig address, and start a spend from one as a PSBT
 *  that the other signers complete with File > Load PSBT. */
class MultisigDialog : public QDialog
{
    Q_OBJECT

public:
    MultisigDialog(const PlatformStyle* platformStyle, WalletModel* walletModel, ClientModel* clientModel, QWidget* parent = nullptr);

private:
    const PlatformStyle* m_platform_style;
    WalletModel* m_wallet_model;
    ClientModel* m_client_model;

    // Create
    QSpinBox* m_required{nullptr};
    QLabel* m_of_n{nullptr};
    QVBoxLayout* m_key_rows{nullptr};
    QList<QLineEdit*> m_keys;
    QLineEdit* m_label{nullptr};
    QPlainTextEdit* m_result{nullptr};

    // Spend
    QLineEdit* m_spend_from{nullptr};
    QLabel* m_spend_balance{nullptr};
    QLineEdit* m_pay_to{nullptr};
    BitcoinAmountField* m_amount{nullptr};

    QWidget* createTab();
    QWidget* spendTab();
    void addKeyRow(const QString& text = QString());
    void updateKeyCount();

private Q_SLOTS:
    void onAddMyKey();
    void onCreate();
    void onSpendFromChanged();
    void onCreatePSBT();
};

#endif // BITCOIN_QT_MULTISIGDIALOG_H
