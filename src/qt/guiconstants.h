// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUICONSTANTS_H
#define BITCOIN_QT_GUICONSTANTS_H

#include <chrono>
#include <cstdint>

using namespace std::chrono_literals;

/* A delay between model updates */
static constexpr auto MODEL_UPDATE_DELAY{250ms};

/* A delay between shutdown pollings */
static constexpr auto SHUTDOWN_POLLING_DELAY{200ms};

/* AskPassphraseDialog -- Maximum passphrase length */
static const int MAX_PASSPHRASE_SIZE = 1024;

/* BitcoinGUI -- Size of icons in status bar */
static const int STATUSBAR_ICONSIZE = 16;

static const bool DEFAULT_SPLASHSCREEN = true;

/* Invalid field background style */
#define STYLE_INVALID "background:#FF4444; border: 1px solid #FF0000; border-radius: 4px;"

/* VALIDITY-PORT: Dark theme colors — refined v2 */
/* Transaction list -- unconfirmed transaction */
#define COLOR_UNCONFIRMED QColor(122, 122, 150)
/* Transaction list -- negative amount */
#define COLOR_NEGATIVE QColor(232, 93, 117)
/* Transaction list -- bare address (without label) */
#define COLOR_BAREADDRESS QColor(122, 122, 150)
/* Transaction list -- TX status decoration - danger, tx needs attention */
#define COLOR_TX_STATUS_DANGER QColor(232, 93, 117)
/* Transaction list -- TX status decoration - default color */
#define COLOR_BLACK QColor(200, 200, 216)

/* Tooltips longer than this (in characters) are converted into rich text,
   so that they can be word-wrapped.
 */
static const int TOOLTIP_WRAP_THRESHOLD = 80;

/* Number of frames in spinner animation */
#define SPINNER_FRAMES 36

#define QAPP_ORG_NAME "Validity"
#define QAPP_ORG_DOMAIN "validity.org"
#define QAPP_APP_NAME_DEFAULT "Validity-Qt"
#define QAPP_APP_NAME_TESTNET "Validity-Qt-testnet"
#define QAPP_APP_NAME_SIGNET "Validity-Qt-signet"
#define QAPP_APP_NAME_REGTEST "Validity-Qt-regtest"

/* One gigabyte (GB) in bytes */
static constexpr uint64_t GB_BYTES{1000000000};

#endif // BITCOIN_QT_GUICONSTANTS_H
