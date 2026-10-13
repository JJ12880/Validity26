// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/forms/ui_overviewpage.h>

#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactionoverviewwidget.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <QAbstractItemDelegate>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QDateTime>
#include <QPainter>
#include <QStatusTipEvent>

#include <algorithm>
#include <map>

#define DECORATION_SIZE 42
#define NUM_ITEMS 5

Q_DECLARE_METATYPE(interfaces::WalletBalances)

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle* _platformStyle, QObject* parent = nullptr)
        : QAbstractItemDelegate(parent), platformStyle(_platformStyle)
    {
        connect(this, &TxViewDelegate::width_changed, this, &TxViewDelegate::sizeHintChanged);
    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const override
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        if (index.data(TransactionTableModel::WatchonlyRole).toBool()) {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(addressRect.left(), addressRect.top(), 16, addressRect.height());
            iconWatchonly = platformStyle->TextColorIcon(iconWatchonly);
            iconWatchonly.paint(painter, watchonlyRect);
            addressRect.setLeft(addressRect.left() + watchonlyRect.width() + 5);
        }

        // VALIDITY-PORT: Muted color for address (left 60%), bright amount (right 40%)
        int amountWidth = addressRect.width() * 2 / 5;
        QRect addrPartRect(addressRect.left(), addressRect.top(), addressRect.width() - amountWidth, addressRect.height());
        QRect amtPartRect(addressRect.left() + addressRect.width() - amountWidth, addressRect.top(), amountWidth, addressRect.height());

        painter->setPen(QColor(122, 122, 150)); // Muted for address
        QRect boundingRect;
        // Elide long addresses
        QString elidedAddr = painter->fontMetrics().elidedText(address, Qt::ElideMiddle, addrPartRect.width());
        painter->drawText(addrPartRect, Qt::AlignLeft | Qt::AlignVCenter, elidedAddr, &boundingRect);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            // VALIDITY-PORT: Muted teal for positive amounts
            foreground = QColor(78, 204, 163);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::SeparatorStyle::ALWAYS);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }

        // VALIDITY-PORT: Draw date (left) and amount (right) on top line
        painter->setPen(QColor(200, 200, 216)); // Warm gray for date
        QRect date_bounding_rect;
        QString dateStr = GUIUtil::dateTimeStr(date);
        painter->drawText(amountRect, Qt::AlignLeft | Qt::AlignVCenter, dateStr, &date_bounding_rect);

        // VALIDITY-PORT: Draw amount right-aligned on bottom line in reserved space
        painter->setPen(foreground);
        QRect amount_bounding_rect;
        painter->drawText(amtPartRect, Qt::AlignRight | Qt::AlignVCenter, amountText, &amount_bounding_rect);

        // VALIDITY-PORT: Don't expand items beyond viewport — let addresses elide instead
        const int minimum_width = 0;
        const auto search = m_minimum_width.find(index.row());
        if (search == m_minimum_width.end() || search->second != minimum_width) {
            m_minimum_width[index.row()] = minimum_width;
            Q_EMIT width_changed(index);
        }

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // VALIDITY-PORT: Use available width, don't expand beyond viewport
        Q_UNUSED(index);
        return {option.rect.width() > 0 ? option.rect.width() : 400, DECORATION_SIZE};
    }

    BitcoinUnit unit{BitcoinUnit::BTC};

Q_SIGNALS:
    //! An intermediate signal for emitting from the `paint() const` member function.
    void width_changed(const QModelIndex& index) const;

private:
    const PlatformStyle* platformStyle;
    mutable std::map<int, int> m_minimum_width;
};

#include <qt/overviewpage.moc>

// VALIDITY: Exposes only the first `limit` rows of its source model (the date-sorted
// TransactionFilterProxy), so the "Recent transactions" view only ever holds NUM_ITEMS
// rows. Replaces hiding view rows with setRowHidden(), which was O(N) and left rows
// beyond the first few visible. Any structural change in the source resets this tiny
// model (cheap at NUM_ITEMS rows), so views never see inserts/removes past the limit.
class TransactionRowLimitProxy : public QAbstractProxyModel
{
public:
    explicit TransactionRowLimitProxy(int limit, QObject* parent = nullptr)
        : QAbstractProxyModel(parent), m_limit(limit) {}

    void setSourceModel(QAbstractItemModel* source) override
    {
        beginResetModel();
        if (sourceModel()) sourceModel()->disconnect(this);
        QAbstractProxyModel::setSourceModel(source);
        if (source) {
            auto begin_reset = [this] { beginResetModel(); };
            auto end_reset = [this] { endResetModel(); };
            connect(source, &QAbstractItemModel::rowsAboutToBeInserted, this, begin_reset);
            connect(source, &QAbstractItemModel::rowsInserted, this, end_reset);
            connect(source, &QAbstractItemModel::rowsAboutToBeRemoved, this, begin_reset);
            connect(source, &QAbstractItemModel::rowsRemoved, this, end_reset);
            connect(source, &QAbstractItemModel::rowsAboutToBeMoved, this, begin_reset);
            connect(source, &QAbstractItemModel::rowsMoved, this, end_reset);
            connect(source, &QAbstractItemModel::layoutAboutToBeChanged, this, begin_reset);
            connect(source, &QAbstractItemModel::layoutChanged, this, end_reset);
            connect(source, &QAbstractItemModel::modelAboutToBeReset, this, begin_reset);
            connect(source, &QAbstractItemModel::modelReset, this, end_reset);
            connect(source, &QAbstractItemModel::dataChanged, this,
                    [this](const QModelIndex& top_left, const QModelIndex& bottom_right, const QVector<int>& roles) {
                        if (top_left.row() >= m_limit) return;
                        const int last = std::min(bottom_right.row(), m_limit - 1);
                        Q_EMIT dataChanged(index(top_left.row(), top_left.column()), index(last, bottom_right.column()), roles);
                    });
        }
        endResetModel();
    }

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid() || row < 0 || row >= rowCount() || column < 0 || column >= columnCount()) return {};
        return createIndex(row, column);
    }
    QModelIndex parent(const QModelIndex&) const override { return {}; }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid() || !sourceModel()) return 0;
        return std::min(sourceModel()->rowCount(), m_limit);
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid() || !sourceModel()) return 0;
        return sourceModel()->columnCount();
    }
    QModelIndex mapToSource(const QModelIndex& proxy_index) const override
    {
        if (!proxy_index.isValid() || !sourceModel()) return {};
        return sourceModel()->index(proxy_index.row(), proxy_index.column());
    }
    QModelIndex mapFromSource(const QModelIndex& source_index) const override
    {
        if (!source_index.isValid() || source_index.row() >= m_limit) return {};
        return index(source_index.row(), source_index.column());
    }

private:
    const int m_limit;
};

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    m_platform_style{platformStyle},
    txdelegate(new TxViewDelegate(platformStyle, this))
{
    ui->setupUi(this);

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = m_platform_style->SingleColorIcon(QStringLiteral(":/icons/warning"));
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, &TransactionOverviewWidget::clicked, this, &OverviewPage::handleTransactionClicked);

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, &QPushButton::clicked, this, &OverviewPage::outOfSyncWarningClicked);
    connect(ui->labelTransactionsStatus, &QPushButton::clicked, this, &OverviewPage::outOfSyncWarningClicked);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if (filter && m_limit_proxy)
        Q_EMIT transactionClicked(filter->mapToSource(m_limit_proxy->mapToSource(index)));
}

void OverviewPage::setPrivacy(bool privacy)
{
    m_privacy = privacy;
    clientModel->getOptionsModel()->setOption(OptionsModel::OptionID::MaskValues, privacy);
    const auto& balances = walletModel->getCachedBalance();
    if (balances.balance != -1) {
        setBalance(balances);
    }

    ui->listTransactions->setVisible(!m_privacy);

    const QString status_tip = m_privacy ? tr("Privacy mode activated for the Overview tab. To unmask the values, uncheck Settings->Mask values.") : "";
    setStatusTip(status_tip);
    QStatusTipEvent event(status_tip);
    QApplication::sendEvent(this, &event);
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(const interfaces::WalletBalances& balances)
{
    BitcoinUnit unit = walletModel->getOptionsModel()->getDisplayUnit();
    unsigned int donation_percentage = walletModel->wallet().getDonationPercentage();
    if (walletModel->wallet().isLegacy()) {
        if (walletModel->wallet().privateKeysDisabled()) {
            ui->labelBalance->setText(BitcoinUnits::formatWithPrivacy(unit, balances.watch_only_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelUnconfirmed->setText(BitcoinUnits::formatWithPrivacy(unit, balances.unconfirmed_watch_only_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelImmature->setText(BitcoinUnits::formatWithPrivacy(unit, balances.immature_watch_only_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelStake->setText(BitcoinUnits::formatWithPrivacy(unit, balances.watch_only_stake, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelTotal->setText(BitcoinUnits::formatWithPrivacy(unit, balances.watch_only_balance + balances.unconfirmed_watch_only_balance + balances.immature_watch_only_balance + balances.watch_only_stake, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
        } else {
            ui->labelBalance->setText(BitcoinUnits::formatWithPrivacy(unit, balances.balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelUnconfirmed->setText(BitcoinUnits::formatWithPrivacy(unit, balances.unconfirmed_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelImmature->setText(BitcoinUnits::formatWithPrivacy(unit, balances.immature_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelStake->setText(BitcoinUnits::formatWithPrivacy(unit, balances.stake, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelTotal->setText(BitcoinUnits::formatWithPrivacy(unit, balances.balance + balances.unconfirmed_balance + balances.immature_balance + balances.stake, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelWatchAvailable->setText(BitcoinUnits::formatWithPrivacy(unit, balances.watch_only_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelWatchPending->setText(BitcoinUnits::formatWithPrivacy(unit, balances.unconfirmed_watch_only_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelWatchImmature->setText(BitcoinUnits::formatWithPrivacy(unit, balances.immature_watch_only_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelWatchStake->setText(BitcoinUnits::formatWithPrivacy(unit, balances.watch_only_stake, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
            ui->labelWatchTotal->setText(BitcoinUnits::formatWithPrivacy(unit, balances.watch_only_balance + balances.unconfirmed_watch_only_balance + balances.immature_watch_only_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
        }
    } else {
        ui->labelBalance->setText(BitcoinUnits::formatWithPrivacy(unit, balances.balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
        ui->labelUnconfirmed->setText(BitcoinUnits::formatWithPrivacy(unit, balances.unconfirmed_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
        ui->labelImmature->setText(BitcoinUnits::formatWithPrivacy(unit, balances.immature_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
        ui->labelStake->setText(BitcoinUnits::formatWithPrivacy(unit, balances.stake, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
        ui->labelTotal->setText(BitcoinUnits::formatWithPrivacy(unit, balances.balance + balances.unconfirmed_balance + balances.immature_balance + balances.stake, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
    }
    ui->labelDonations->setText((m_privacy ? QString::fromStdString("#") : (QString::number(donation_percentage) + "%")) + " of stake rewards");
    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = balances.immature_balance != 0;
    bool showStake = balances.stake != 0;
    bool showWatchOnlyImmature = balances.immature_watch_only_balance != 0;
    bool showWatchOnlyStake = balances.watch_only_stake != 0;
    bool showDonations = donation_percentage != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(!walletModel->wallet().privateKeysDisabled() && showWatchOnlyImmature); // show watch-only immature balance
    ui->labelStake->setVisible(showStake || showWatchOnlyStake);
    ui->labelStakeText->setVisible(showStake || showWatchOnlyStake);
    ui->labelWatchStake->setVisible(!walletModel->wallet().privateKeysDisabled() && showWatchOnlyStake); // show watch-only stake balance
    ui->labelDonations->setVisible(showDonations);
    ui->labelDonationsText->setVisible(showDonations);
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance

    if (!showWatchOnly)
    {
        ui->labelWatchImmature->hide();
        ui->labelWatchStake->hide();
    }
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model) {
        // Show warning, for example if this is a prerelease version
        connect(model, &ClientModel::alertsChanged, this, &OverviewPage::updateAlerts);
        updateAlerts(model->getStatusBarWarnings());

        connect(model->getOptionsModel(), &OptionsModel::useEmbeddedMonospacedFontChanged, this, &OverviewPage::setMonospacedFont);
        setMonospacedFont(model->getOptionsModel()->getUseEmbeddedMonospacedFont());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        // VALIDITY: Dynamic sort/filter is required so new transactions land at the top and
        // status changes (e.g. orphaned stakes) re-filter. It is cheap now that
        // TransactionTableModel::updateConfirmations() only invalidates rows that can change.
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        // VALIDITY: Only expose the NUM_ITEMS most recent rows to the view.
        m_limit_proxy.reset(new TransactionRowLimitProxy(NUM_ITEMS));
        m_limit_proxy->setSourceModel(filter.get());

        ui->listTransactions->setModel(m_limit_proxy.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getCachedBalance());
        connect(model, &WalletModel::balanceChanged, this, &OverviewPage::setBalance);

        connect(model->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &OverviewPage::updateDisplayUnit);

        interfaces::Wallet& wallet = model->wallet();
        updateWatchOnlyLabels(wallet.haveWatchOnly() && !wallet.privateKeysDisabled());
        connect(model, &WalletModel::notifyWatchonlyChanged, [this](bool showWatchOnly) {
            updateWatchOnlyLabels(showWatchOnly && !walletModel->wallet().privateKeysDisabled());
        });
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void OverviewPage::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::PaletteChange) {
        QIcon icon = m_platform_style->SingleColorIcon(QStringLiteral(":/icons/warning"));
        ui->labelTransactionsStatus->setIcon(icon);
        ui->labelWalletStatus->setIcon(icon);
    }

    QWidget::changeEvent(e);
}

void OverviewPage::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        const auto& balances = walletModel->getCachedBalance();
        if (balances.balance != -1) {
            setBalance(balances);
        }

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::setMonospacedFont(bool use_embedded_font)
{
    QFont f = GUIUtil::fixedPitchFont(use_embedded_font);
    f.setWeight(QFont::Bold);
    ui->labelBalance->setFont(f);
    ui->labelUnconfirmed->setFont(f);
    ui->labelImmature->setFont(f);
    ui->labelStake->setFont(f);
    ui->labelTotal->setFont(f);
    ui->labelWatchAvailable->setFont(f);
    ui->labelWatchPending->setFont(f);
    ui->labelWatchImmature->setFont(f);
    ui->labelWatchStake->setFont(f);
    ui->labelWatchTotal->setFont(f);
}
