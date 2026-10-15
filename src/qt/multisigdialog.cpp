// Copyright (c) 2026 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/multisigdialog.h>

#include <qt/bitcoinamountfield.h>
#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/psbtoperationsdialog.h>
#include <qt/walletmodel.h>

#include <interfaces/wallet.h>
#include <key_io.h>
#include <outputtype.h>
#include <psbt.h>
#include <script/script.h>
#include <util/error.h>
#include <util/strencodings.h>
#include <util/translation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

static constexpr int MAX_MULTISIG_KEYS = 15; // most a standard P2SH spend allows

MultisigDialog::MultisigDialog(const PlatformStyle* platformStyle, WalletModel* walletModel, ClientModel* clientModel, QWidget* parent)
    : QDialog(parent, GUIUtil::dialog_flags)
    , m_platform_style(platformStyle)
    , m_wallet_model(walletModel)
    , m_client_model(clientModel)
{
    setWindowTitle(tr("Multisig"));
    setMinimumWidth(620);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(createTab(), tr("New address"));
    tabs->addTab(spendTab(), tr("Spend"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

QWidget* MultisigDialog::createTab()
{
    auto* page = new QWidget(this);

    m_required = new QSpinBox(page);
    m_required->setRange(1, 2);
    m_required->setValue(2);
    m_of_n = new QLabel(page);
    auto* requiredRow = new QHBoxLayout;
    requiredRow->addWidget(new QLabel(tr("Required signatures:"), page));
    requiredRow->addWidget(m_required);
    requiredRow->addWidget(m_of_n);
    requiredRow->addStretch();

    m_key_rows = new QVBoxLayout;
    addKeyRow();
    addKeyRow();

    auto* addKey = new QPushButton(tr("+ Add key"), page);
    auto* addMyKey = new QPushButton(tr("Add one of my keys"), page);
    addMyKey->setToolTip(tr("Make a new address in this wallet and add its public key"));
    connect(addKey, &QPushButton::clicked, this, [this] { addKeyRow(); });
    connect(addMyKey, &QPushButton::clicked, this, &MultisigDialog::onAddMyKey);
    auto* keyButtons = new QHBoxLayout;
    keyButtons->addWidget(addKey);
    keyButtons->addWidget(addMyKey);
    keyButtons->addStretch();

    m_label = new QLineEdit(page);
    m_label->setPlaceholderText(tr("e.g. Escrow with Bob"));
    auto* labelRow = new QHBoxLayout;
    labelRow->addWidget(new QLabel(tr("Label:"), page));
    labelRow->addWidget(m_label, 1);

    auto* create = new QPushButton(tr("Create address"), page);
    create->setDefault(true);
    connect(create, &QPushButton::clicked, this, &MultisigDialog::onCreate);
    auto* createRow = new QHBoxLayout;
    createRow->addStretch();
    createRow->addWidget(create);

    m_result = new QPlainTextEdit(page);
    m_result->setReadOnly(true);
    m_result->setPlaceholderText(tr("Every signer enters the same public keys in the same order to get the same address. "
                                    "Each key is a public key (hex) or an address of this wallet."));
    m_result->setMaximumHeight(130);

    auto* layout = new QVBoxLayout(page);
    layout->addLayout(requiredRow);
    layout->addLayout(m_key_rows);
    layout->addLayout(keyButtons);
    layout->addLayout(labelRow);
    layout->addLayout(createRow);
    layout->addWidget(m_result);
    updateKeyCount();
    return page;
}

void MultisigDialog::addKeyRow(const QString& text)
{
    if (m_keys.size() >= MAX_MULTISIG_KEYS) {
        QMessageBox::information(this, windowTitle(), tr("A multisig address can have at most %1 keys.").arg(MAX_MULTISIG_KEYS));
        return;
    }
    auto* edit = new QLineEdit(text, this);
    edit->setPlaceholderText(tr("Public key (hex) or an address of this wallet"));
    m_keys.append(edit);
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("Key %1:").arg(m_keys.size()), this));
    row->addWidget(edit, 1);
    m_key_rows->addLayout(row);
    updateKeyCount();
}

void MultisigDialog::updateKeyCount()
{
    m_required->setMaximum(m_keys.size());
    m_of_n->setText(tr("of %1").arg(m_keys.size()));
}

void MultisigDialog::onAddMyKey()
{
    auto dest = m_wallet_model->wallet().getNewDestination(OutputType::LEGACY, m_label->text().trimmed().toStdString());
    if (!dest) {
        QMessageBox::warning(this, windowTitle(), QString::fromStdString(util::ErrorString(dest).translated));
        return;
    }
    const PKHash* pkh = std::get_if<PKHash>(&*dest);
    CPubKey pubkey;
    if (!pkh || !m_wallet_model->wallet().getPubKey(GetScriptForDestination(*dest), ToKeyID(*pkh), pubkey)) {
        QMessageBox::warning(this, windowTitle(), tr("Could not read the new key's public key."));
        return;
    }
    const QString hex = QString::fromStdString(HexStr(pubkey));
    for (QLineEdit* edit : m_keys) {
        if (edit->text().trimmed().isEmpty()) {
            edit->setText(hex);
            return;
        }
    }
    addKeyRow(hex);
}

void MultisigDialog::onCreate()
{
    std::vector<std::string> keys;
    for (QLineEdit* edit : m_keys) {
        const QString key = edit->text().trimmed();
        if (!key.isEmpty()) keys.push_back(key.toStdString());
    }
    std::string address;
    std::vector<std::string> pubkeys;
    bilingual_str error;
    if (!m_wallet_model->wallet().addMultisigAddress(m_required->value(), keys, m_label->text().trimmed().toStdString(), address, pubkeys, error)) {
        QMessageBox::warning(this, windowTitle(), QString::fromStdString(error.translated));
        return;
    }
    QString text = tr("Address: %1").arg(QString::fromStdString(address)) + "\n";
    text += tr("%1 of %2 signatures required. Public keys, in order:").arg(m_required->value()).arg(pubkeys.size()) + "\n";
    for (const std::string& pubkey : pubkeys) text += QString::fromStdString(pubkey) + "\n";
    m_result->setPlainText(text);
    m_spend_from->setText(QString::fromStdString(address));
}

QWidget* MultisigDialog::spendTab()
{
    auto* page = new QWidget(this);

    m_spend_from = new QLineEdit(page);
    m_spend_from->setPlaceholderText(tr("Multisig address added to this wallet"));
    m_spend_balance = new QLabel(page);
    connect(m_spend_from, &QLineEdit::editingFinished, this, &MultisigDialog::onSpendFromChanged);
    connect(m_spend_from, &QLineEdit::textChanged, this, &MultisigDialog::onSpendFromChanged);

    m_pay_to = new QLineEdit(page);
    m_pay_to->setPlaceholderText(tr("X... or Q... address"));
    m_amount = new BitcoinAmountField(page);
    if (m_wallet_model->getOptionsModel()) m_amount->setDisplayUnit(m_wallet_model->getOptionsModel()->getDisplayUnit());

    auto* form = new QFormLayout;
    form->addRow(tr("From:"), m_spend_from);
    form->addRow(QString(), m_spend_balance);
    form->addRow(tr("Pay to:"), m_pay_to);
    form->addRow(tr("Amount:"), m_amount);

    auto* note = new QLabel(tr("Change goes back to the multisig address. This wallet signs with any keys it holds, "
                               "then you send the PSBT to the other signers (File > Load PSBT) to finish and broadcast."), page);
    note->setWordWrap(true);

    auto* create = new QPushButton(tr("Create PSBT…"), page);
    connect(create, &QPushButton::clicked, this, &MultisigDialog::onCreatePSBT);
    auto* createRow = new QHBoxLayout;
    createRow->addStretch();
    createRow->addWidget(create);

    auto* layout = new QVBoxLayout(page);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addLayout(createRow);
    layout->addStretch();
    return page;
}

void MultisigDialog::onSpendFromChanged()
{
    const CTxDestination dest = DecodeDestination(m_spend_from->text().trimmed().toStdString());
    if (!std::holds_alternative<ScriptHash>(dest)) {
        m_spend_balance->setText(m_spend_from->text().trimmed().isEmpty() ? QString() : tr("Not a P2SH address"));
        return;
    }
    CAmount total = 0;
    const auto coins = m_wallet_model->wallet().listCoinsTo(dest);
    for (const auto& coin : coins) total += coin.second;
    const BitcoinUnit unit = m_wallet_model->getOptionsModel() ? m_wallet_model->getOptionsModel()->getDisplayUnit() : BitcoinUnit::BTC;
    m_spend_balance->setText(tr("Available: %1 in %n output(s)", "", coins.size()).arg(BitcoinUnits::formatWithUnit(unit, total)));
}

void MultisigDialog::onCreatePSBT()
{
    const CTxDestination from = DecodeDestination(m_spend_from->text().trimmed().toStdString());
    if (!std::holds_alternative<ScriptHash>(from)) {
        QMessageBox::warning(this, windowTitle(), tr("Enter the multisig (P2SH) address to spend from."));
        return;
    }
    const CTxDestination to = DecodeDestination(m_pay_to->text().trimmed().toStdString());
    if (!std::holds_alternative<PKHash>(to) && !std::holds_alternative<ScriptHash>(to)) {
        QMessageBox::warning(this, windowTitle(), tr("Enter a valid P2PKH or P2SH address to pay to."));
        return;
    }
    bool amount_ok = false;
    const CAmount amount = m_amount->value(&amount_ok);
    if (!amount_ok || amount <= 0) {
        QMessageBox::warning(this, windowTitle(), tr("Enter an amount."));
        return;
    }
    const auto coins = m_wallet_model->wallet().listCoinsTo(from);
    if (coins.empty()) {
        QMessageBox::warning(this, windowTitle(), tr("This wallet has no confirmed coins at that address."));
        return;
    }

    // Spend only the multisig's own outputs, and send the change back to it.
    wallet::CCoinControl coin_control;
    coin_control.fAllowWatchOnly = true;
    coin_control.m_allow_other_inputs = false;
    coin_control.destChange = from;
    for (const auto& coin : coins) coin_control.Select(coin.first);

    int change_pos = -1;
    CAmount fee = 0;
    auto tx = m_wallet_model->wallet().createTransaction({wallet::CRecipient{to, amount, /*fSubtractFeeFromAmount=*/false}},
                                                         coin_control, /*sign=*/false, change_pos, fee);
    if (!tx) {
        QMessageBox::warning(this, windowTitle(), QString::fromStdString(util::ErrorString(tx).translated));
        return;
    }

    PartiallySignedTransaction psbtx{CMutableTransaction{**tx}};
    bool complete = false;
    TransactionError err = m_wallet_model->wallet().fillPSBT(SIGHASH_ALL, /*sign=*/false, /*bip32derivs=*/true, /*n_signed=*/nullptr, psbtx, complete);
    if (err != TransactionError::OK) {
        QMessageBox::warning(this, windowTitle(), QString::fromStdString(TransactionErrorString(err).translated));
        return;
    }

    // The PSBT dialog signs with this wallet's keys (asking for a full unlock if needed),
    // then copies, saves or broadcasts.
    auto* dlg = new PSBTOperationsDialog(this, m_wallet_model, m_client_model);
    dlg->openWithPSBT(psbtx);
    GUIUtil::ShowModalDialogAsynchronously(dlg);
}
