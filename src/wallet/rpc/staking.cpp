// Copyright (c) 2014-2023 The Validity developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/util.h>
#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <wallet/rpc/util.h>
#include <wallet/rpc/staking.h>
#include <wallet/receive.h>
#include <wallet/wallet.h>
#include <node/context.h>
#include <node/miner.h>
#include <net.h>
#include <net_processing.h>
#include <key_io.h> // For EncodeDestination
#include <pow.h> // For GetNextTargetRequired
#include <clientversion.h>
#include <version.h>
#include <warnings.h>

#include <univalue.h>

using node::BlockAssembler;

namespace wallet {

static RPCHelpMan getstakinginfo()
{
    return RPCHelpMan{"getstakinginfo",
                "\nReturns an object containing staking-related information.",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::BOOL, "enabled", "'true' if staking is enabled"},
                        {RPCResult::Type::BOOL, "staking", "'true' if wallet is currently staking"},
                        {RPCResult::Type::STR, "errors", "error messages"},
                        {RPCResult::Type::NUM, "pooledtx", "The size of the mempool"},
                        {RPCResult::Type::NUM, "difficulty", "The current difficulty"},
                        {RPCResult::Type::NUM, "search-interval", "The staker search interval"},
                        {RPCResult::Type::NUM, "weight", "The staker weight"},
                        {RPCResult::Type::NUM, "netstakeweight", "Network stake weight"},
                        {RPCResult::Type::NUM, "expectedtime", "Expected time to earn reward"},
                        {RPCResult::Type::OBJ, "stakedonation", "Share of each stake reward paid to another address (-stakedonationaddress / -stakedonationpercent)",
                        {
                            {RPCResult::Type::NUM, "percent", "Percentage of each stake reward donated; 0 when disabled"},
                            {RPCResult::Type::STR, "address", /*optional=*/true, "Address receiving the donation (only when enabled)"},
                        }},
                        {RPCResult::Type::STR_HEX, "staketag", "Data published with each stake (-staketag); empty when disabled"},
                    }
                },
                RPCExamples{
                    HelpExampleCli("getstakinginfo", "")
            + HelpExampleRpc("getstakinginfo", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return NullUniValue;

    uint64_t nWeight = 0;
    uint64_t lastCoinStakeSearchInterval = 0;
    UniValue donation(UniValue::VOBJ);
    std::vector<unsigned char> stake_tag;

    if (pwallet)
    {
        LOCK(pwallet->cs_wallet);
        nWeight = pwallet->GetStakeWeight();
        lastCoinStakeSearchInterval = pwallet->m_enabled_staking ? pwallet->m_last_coin_stake_search_interval : 0;
        // Read under the same cs_wallet hold: cs_wallet must never be taken while cs_main is held
        // (the GUI locks cs_wallet then cs_main, see WalletImpl::tryGetTxStatus).
        donation.pushKV("percent", (uint64_t)pwallet->m_donation_percentage);
        if (pwallet->m_donation_percentage > 0) donation.pushKV("address", EncodeDestination(pwallet->m_donation_destination));
        stake_tag = pwallet->m_stake_tag;
    }

    const CTxMemPool& mempool = pwallet->chain().mempool();
    ChainstateManager& chainman = pwallet->chain().chainman();
    LOCK(cs_main);
    const CChain& active_chain = chainman.ActiveChain();

    UniValue obj(UniValue::VOBJ);

    uint64_t nNetworkWeight = 1.1429 * GetPoSKernelPS(chainman);
    bool staking = lastCoinStakeSearchInterval && nWeight;

    const Consensus::Params& consensusParams = Params().GetConsensus();
    int64_t nTargetSpacing = consensusParams.nTargetSpacing;
    uint64_t nExpectedTime = staking ? 1.0455 * nTargetSpacing * nNetworkWeight / nWeight : 0;

    obj.pushKV("enabled", pwallet->m_enabled_staking.load());
    obj.pushKV("staking", staking);

    obj.pushKV("blocks", active_chain.Height());
    if (BlockAssembler::m_last_block_weight) obj.pushKV("currentblockweight", *BlockAssembler::m_last_block_weight);
    if (BlockAssembler::m_last_block_num_txs) obj.pushKV("currentblocktx", *BlockAssembler::m_last_block_num_txs);
    obj.pushKV("pooledtx", (uint64_t)mempool.size());

    obj.pushKV("difficulty", GetDifficulty(GetLastBlockIndex(chainman.m_best_header, true)));

    obj.pushKV("search-interval", (int)lastCoinStakeSearchInterval);
    obj.pushKV("weight", (uint64_t)nWeight);
    obj.pushKV("netstakeweight", (uint64_t)nNetworkWeight);
    obj.pushKV("expectedtime", nExpectedTime);

    obj.pushKV("stakedonation", donation);
    obj.pushKV("staketag", HexStr(stake_tag));

    obj.pushKV("chain", chainman.GetParams().GetChainTypeString());
    obj.pushKV("warnings", GetWarnings(false).original);
    return obj;
},
    };
}

static RPCHelpMan staking()
{
    return RPCHelpMan{"staking",
            "Gets or sets the current staking configuration.\n"
            "When called without an argument, returns the current status of staking.\n"
            "When called with an argument, enables or disables staking.\n",
            {
                {"generate", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED, "To enable or disable staking."},

            },
            RPCResult{
                RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::BOOL, "staking", "if staking is active or not. false: inactive, true: active"},
                }
            },
            RPCExamples{
                HelpExampleCli("staking", "true")
                + HelpExampleRpc("staking", "true")
            },
            [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return NullUniValue;

    std::string error = "";
    if (request.params.size() > 0)
    {
        if (request.params[0].get_bool() && node::CanStake())
        {
            if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
                error = "The wallet can't contain any private keys";
            } else if (pwallet->IsWalletFlagSet(WALLET_FLAG_BLANK_WALLET)) {
                error = "The wallet is blank";
            }
            if (!pwallet->m_enabled_staking)
                StartStake(*pwallet);
        }
        else {
            StopStake(*pwallet);
        }
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("staking", pwallet->m_enabled_staking.load());
    if (!error.empty()) {
        result.pushKV("error", error);
    }
    return result;
},
    };
}

static RPCHelpMan reservebalance()
{
    return RPCHelpMan{"reservebalance",
            "\nSet reserve amount not participating in network protection."
            "\nIf no parameters provided current setting is printed.\n",
            {
                {"reserve", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED,"is true or false to turn balance reserve on or off."},
                {"amount", RPCArg::Type::AMOUNT, RPCArg::Optional::OMITTED, "is a real and rounded to cent."},
            },
            RPCResult{
                RPCResult::Type::OBJ, "", "",
                {
                    {RPCResult::Type::BOOL, "reserve", "Balance reserve on or off"},
                    {RPCResult::Type::STR_AMOUNT, "amount", "Amount reserve rounded to cent"}
                }
            },
             RPCExamples{
            "\nSet reserve balance to 100\n"
            + HelpExampleCli("reservebalance", "true 100") +
            "\nSet reserve balance to 0\n"
            + HelpExampleCli("reservebalance", "false") +
            "\nGet reserve balance\n"
            + HelpExampleCli("reservebalance", "")			},
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return NullUniValue;

    if (request.params.size() > 0)
    {
        bool fReserve = request.params[0].get_bool();
        if (fReserve)
        {
            if (request.params.size() == 1)
                throw std::runtime_error("must provide amount to reserve balance.\n");
            int64_t nAmount = AmountFromValue(request.params[1]);
            nAmount = (nAmount / CENT) * CENT;  // round to cent
            if (nAmount < 0)
                throw std::runtime_error("amount cannot be negative.\n");
            pwallet->m_reserve_balance = nAmount;
        }
        else
        {
            if (request.params.size() > 1)
                throw std::runtime_error("cannot specify amount to turn off reserve.\n");
            pwallet->m_reserve_balance = 0;
        }
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("reserve", (pwallet->m_reserve_balance > 0));
    result.pushKV("amount", ValueFromAmount(pwallet->m_reserve_balance));
    return result;
},
    };
}

static RPCHelpMan checkkernel()
{
    return RPCHelpMan{"checkkernel",
                "\nCheck if one of given inputs is a kernel input at the moment.\n",
                {
                    {"inputs", RPCArg::Type::ARR, RPCArg::Optional::NO, "The inputs",
                        {
                            {"", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                                {
                                    {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The transaction id"},
                                    {"vout", RPCArg::Type::NUM, RPCArg::Optional::NO, "The output number"},
                                    {"sequence", RPCArg::Type::NUM, RPCArg::DefaultHint{"depends on the value of the 'locktime' argument"}, "The sequence number"},
                                },
                            },
                        },
                    },
                    {"createblocktemplate", RPCArg::Type::BOOL, RPCArg::Default{false}, "Create block template?"},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::BOOL, "found", "?"},
                        {RPCResult::Type::OBJ, "kernel", "",
                            {
                                {RPCResult::Type::STR_HEX, "txid", "The transaction hash in hex"},
                                {RPCResult::Type::NUM, "vout", "?"},
                                {RPCResult::Type::NUM, "time", "?"},
                            }},
                        {RPCResult::Type::STR_HEX, "blocktemplate", "?"},
                        {RPCResult::Type::NUM, "blocktemplatefees", "?"},
                    },
                },
                RPCExamples{
                HelpExampleCli("checkkernel", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"false\"")
                + HelpExampleCli("checkkernel", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"true\"")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return NullUniValue;

    const CTxMemPool& mempool = pwallet->chain().mempool();
    ChainstateManager& chainman = pwallet->chain().chainman();
    LOCK(cs_main);
    const CChain& active_chain = chainman.ActiveChain();
    Chainstate& active_chainstate = chainman.ActiveChainstate();

    UniValue inputs = request.params[0].get_array();
    bool fCreateBlockTemplate = request.params.size() > 1 ? request.params[1].get_bool() : false;

    if (!Params().IsTestChain()) {
        if (pwallet->chain().getNodeCount(ConnectionDirection::Both) == 0) {
            throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, PACKAGE_NAME " is not connected!");
        }

        if (chainman.IsInitialBlockDownload()) {
            throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, PACKAGE_NAME " is in initial sync and waiting for blocks...");
        }
    }

    COutPoint kernel;
    CBlockIndex* pindexPrev = active_chain.Tip();
    unsigned int nBits = GetNextTargetRequired(pindexPrev, Params().GetConsensus(), true);
    int64_t nTime = GetAdjustedTimeSeconds();
    nTime &= ~Params().GetConsensus().nStakeTimestampMask;

    for (unsigned int idx = 0; idx < inputs.size(); idx++) {
        const UniValue& o = inputs[idx].get_obj();

        const UniValue& txid_v = o.find_value("txid");
        if (!txid_v.isStr())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing txid key");
        string txid = txid_v.get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        const UniValue& vout_v = o.find_value("vout");
        if (!vout_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.getInt<int>();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        COutPoint cInput(uint256S(txid), nOutput);
        if (CheckKernel(pindexPrev, nBits, nTime, cInput, active_chainstate.CoinsTip()))
        {
            kernel = cInput;
            break;
        }
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("found", !kernel.IsNull());

    if (kernel.IsNull())
        return result;

    UniValue oKernel(UniValue::VOBJ);
    oKernel.pushKV("txid", kernel.hash.GetHex());
    oKernel.pushKV("vout", (int64_t)kernel.n);
    oKernel.pushKV("time", nTime);
    result.pushKV("kernel", oKernel);

    if (!fCreateBlockTemplate)
        return result;

    if (!pwallet->IsLocked())
        pwallet->TopUpKeyPool();

    bool fPoSCancel = false;
    int64_t nFees;
    std::unique_ptr<node::CBlockTemplate> pblocktemplate(BlockAssembler{active_chainstate, &mempool}.CreateNewBlock(CScript(), nullptr, &fPoSCancel, &nFees));
    if (!pblocktemplate.get())
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Couldn't create new block");

    CBlock *pblock = &pblocktemplate->block;
    CMutableTransaction coinstakeTx(*pblock->vtx[0]);
    pblock->nTime = coinstakeTx.nTime = nTime;
    pblock->vtx[0] = MakeTransactionRef(std::move(coinstakeTx));

    CDataStream ss(SER_DISK);
    ss << RPCTxSerParams(*pblock);

    result.pushKV("blocktemplate", HexStr(ss));
    result.pushKV("blocktemplatefees", nFees);

    if (!pwallet->CanGetAddresses(true)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: This wallet has no available keys");
    }

    // Prepare reserve destination
    OutputType output_type = pwallet->m_default_change_type ? *pwallet->m_default_change_type : pwallet->m_default_address_type;
    auto op_dest = pwallet->GetNewChangeDestination(output_type);
    if (!op_dest) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Keypool ran out, please call keypoolrefill first");
    }
    std::vector<valtype> vSolutionsTmp;
    CScript scriptPubKeyTmp = GetScriptForDestination(*op_dest);
    Solver(scriptPubKeyTmp, vSolutionsTmp);
    std::unique_ptr<SigningProvider> provider = pwallet->GetSolvingProvider(scriptPubKeyTmp);
    if (!provider) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: failed to get signing provider");
    }
    CKeyID ckey = CKeyID(uint160(vSolutionsTmp[0]));
    CPubKey pkey;
    if (!provider.get()->GetPubKey(ckey, pkey)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: failed to get key");
    }
    result.pushKV("blocktemplatesignkey", HexStr(pkey));

    return result;
},
    };
}

static RPCHelpMan getnodestate()
{
    return RPCHelpMan{"getnodestate",
                "\nReturns combined node state in a single call.\n"
                "Combines data from getblockchaininfo, getnetworkinfo, getwalletinfo, getstakinginfo,\n"
                "peer count, and wallet lock status into one response.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::OBJ, "blockchain", "",
                        {
                            {RPCResult::Type::STR, "chain", "Current network name"},
                            {RPCResult::Type::NUM, "blocks", "Current block height"},
                            {RPCResult::Type::NUM, "headers", "Number of headers"},
                            {RPCResult::Type::STR_HEX, "bestblockhash", "The hash of the best block"},
                            {RPCResult::Type::NUM, "difficulty", "The current difficulty"},
                            {RPCResult::Type::BOOL, "initialblockdownload", "Whether initial block download is in progress"},
                        }},
                        {RPCResult::Type::OBJ, "network", "",
                        {
                            {RPCResult::Type::NUM, "version", "The server version"},
                            {RPCResult::Type::NUM, "protocolversion", "The protocol version"},
                            {RPCResult::Type::NUM, "connections", "Total connections"},
                            {RPCResult::Type::NUM, "connections_in", "Inbound connections"},
                            {RPCResult::Type::NUM, "connections_out", "Outbound connections"},
                        }},
                        {RPCResult::Type::OBJ, "wallet", "",
                        {
                            {RPCResult::Type::STR, "name", "Wallet name"},
                            {RPCResult::Type::STR_AMOUNT, "balance", "Confirmed balance"},
                            {RPCResult::Type::STR_AMOUNT, "unconfirmed_balance", "Unconfirmed balance"},
                            {RPCResult::Type::STR_AMOUNT, "immature_balance", "Immature balance"},
                            {RPCResult::Type::STR_AMOUNT, "stake", "Staking balance"},
                            {RPCResult::Type::NUM, "txcount", "Total number of transactions"},
                            {RPCResult::Type::BOOL, "locked", "Whether the wallet is locked"},
                        }},
                        {RPCResult::Type::OBJ, "staking", "",
                        {
                            {RPCResult::Type::BOOL, "enabled", "Whether staking is enabled"},
                            {RPCResult::Type::BOOL, "staking", "Whether actively staking"},
                            {RPCResult::Type::NUM, "weight", "Staking weight"},
                            {RPCResult::Type::NUM, "netstakeweight", "Network stake weight"},
                            {RPCResult::Type::NUM, "expectedtime", "Expected time to earn reward"},
                            {RPCResult::Type::NUM, "difficulty", "Staking difficulty"},
                        }},
                    }
                },
                RPCExamples{
                    HelpExampleCli("getnodestate", "")
            + HelpExampleRpc("getnodestate", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return NullUniValue;

    ChainstateManager& chainman = pwallet->chain().chainman();
    const CTxMemPool& mempool = pwallet->chain().mempool();

    UniValue result(UniValue::VOBJ);

    // --- Blockchain info ---
    UniValue blockchain(UniValue::VOBJ);
    {
        LOCK(cs_main);
        const CChain& active_chain = chainman.ActiveChain();
        const CBlockIndex* tip = active_chain.Tip();

        blockchain.pushKV("chain", chainman.GetParams().GetChainTypeString());
        blockchain.pushKV("blocks", active_chain.Height());
        blockchain.pushKV("headers", chainman.m_best_header ? chainman.m_best_header->nHeight : -1);
        blockchain.pushKV("bestblockhash", tip ? tip->GetBlockHash().GetHex() : "");
        blockchain.pushKV("difficulty", GetDifficulty(GetLastBlockIndex(tip, false)));
        blockchain.pushKV("initialblockdownload", chainman.IsInitialBlockDownload());
    }
    result.pushKV("blockchain", blockchain);

    // --- Network info ---
    UniValue network(UniValue::VOBJ);
    network.pushKV("version", CLIENT_VERSION);
    network.pushKV("protocolversion", PROTOCOL_VERSION);
    {
        size_t nTotal = pwallet->chain().getNodeCount(ConnectionDirection::Both);
        size_t nIn = pwallet->chain().getNodeCount(ConnectionDirection::In);
        size_t nOut = pwallet->chain().getNodeCount(ConnectionDirection::Out);
        network.pushKV("connections", (int)nTotal);
        network.pushKV("connections_in", (int)nIn);
        network.pushKV("connections_out", (int)nOut);
    }
    result.pushKV("network", network);

    // --- Wallet info ---
    UniValue wallet(UniValue::VOBJ);
    {
        LOCK(pwallet->cs_wallet);
        wallet.pushKV("name", pwallet->GetName());
        const auto bal = GetBalance(*pwallet);
        wallet.pushKV("balance", ValueFromAmount(bal.m_mine_trusted));
        wallet.pushKV("unconfirmed_balance", ValueFromAmount(bal.m_mine_untrusted_pending));
        wallet.pushKV("immature_balance", ValueFromAmount(bal.m_mine_immature));
        wallet.pushKV("stake", ValueFromAmount(bal.m_mine_stake));
        wallet.pushKV("txcount", (int)pwallet->mapWallet.size());
        wallet.pushKV("locked", pwallet->IsCrypted() && pwallet->IsLocked());
    }
    result.pushKV("wallet", wallet);

    // --- Staking info ---
    UniValue stakingObj(UniValue::VOBJ);
    {
        uint64_t nWeight = 0;
        uint64_t lastCoinStakeSearchInterval = 0;
        {
            LOCK(pwallet->cs_wallet);
            nWeight = pwallet->GetStakeWeight();
            lastCoinStakeSearchInterval = pwallet->m_enabled_staking ? pwallet->m_last_coin_stake_search_interval : 0;
        }

        LOCK(cs_main);
        uint64_t nNetworkWeight = 1.1429 * GetPoSKernelPS(chainman);
        bool fStaking = lastCoinStakeSearchInterval && nWeight;

        const Consensus::Params& consensusParams = Params().GetConsensus();
        int64_t nTargetSpacing = consensusParams.nTargetSpacing;
        uint64_t nExpectedTime = fStaking ? 1.0455 * nTargetSpacing * nNetworkWeight / nWeight : 0;

        stakingObj.pushKV("enabled", pwallet->m_enabled_staking.load());
        stakingObj.pushKV("staking", fStaking);
        stakingObj.pushKV("weight", (uint64_t)nWeight);
        stakingObj.pushKV("netstakeweight", (uint64_t)nNetworkWeight);
        stakingObj.pushKV("expectedtime", nExpectedTime);
        stakingObj.pushKV("difficulty", GetDifficulty(GetLastBlockIndex(chainman.m_best_header, true)));
    }
    result.pushKV("staking", stakingObj);

    return result;
},
    };
}

static RPCHelpMan iswalletlocked()
{
    return RPCHelpMan{"iswalletlocked",
                "\nReturns whether the wallet is locked.\n",
                {},
                RPCResult{
                    RPCResult::Type::BOOL, "", "true if the wallet is locked, false otherwise"
                },
                RPCExamples{
                    HelpExampleCli("iswalletlocked", "")
            + HelpExampleRpc("iswalletlocked", "")
                },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
{
    std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
    if (!pwallet) return NullUniValue;

    LOCK(pwallet->cs_wallet);
    return pwallet->IsCrypted() && pwallet->IsLocked();
},
    };
}

Span<const CRPCCommand> GetStakingRPCCommands()
{
// clang-format off
static const CRPCCommand commands[] =
{ //  category              actor (function)
  //  ------------------    ------------------------
    { "staking",            &getstakinginfo,                 },
    { "staking",            &reservebalance,                 },
    { "staking",            &staking,                        },
    { "staking",            &checkkernel,                    },
    { "staking",            &getnodestate,                   },
    { "staking",            &iswalletlocked,                 },
};
// clang-format on
    return commands;
}

} // namespace wallet
