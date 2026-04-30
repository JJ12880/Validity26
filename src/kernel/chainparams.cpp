// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    // Genesis block

    // MainNet:

    //CBlock(hash=000000770c6aea829bb1ace7b06497f71799a6358e0e292740c4f9443a17bfb6, ver=1, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=a02b3388d9e8529bb0136ed3e1b823e808cbe128050e5986bf2a5a0d2c71a826, nTime=1431857735, nBits=1e0fffff, nNonce=109996, vtx=1, vchBlockSig=)
    //  Coinbase(hash=a02b3388d9, nTime=1431857735, ver=1, vin.size=1, vout.size=1, nLockTime=0)
    //    CTxIn(COutPoint(0000000000, 4294967295), coinbase 00012a1232303135207852616f6e20636f696e2e21)
    //    CTxOut(empty)
    //  vMerkleTree: a02b3388d9

    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.nTime = nTime;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 0 << CScriptNum(42) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "2015 xRadon coin.!";
    const CScript genesisOutputScript = CScript();
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nMaxReorganizationDepth = 500;
        // VALIDITY-PORT: All consensus constants from validity-ref/src/chainparams.cpp
        consensus.CSVHeight = std::numeric_limits<int>::max(); // CSV not activated on Validity
        consensus.SegwitHeight = std::numeric_limits<int>::max();
        consensus.MinBIP9WarningHeight = std::numeric_limits<int>::max();
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV2 = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nTargetTimespan = 1 * 60; // 1 min
        consensus.nTargetTimespanNEW = 15 * 60; // 15 min (after block 48)
        consensus.nTargetSpacingV1 = 60;
        consensus.nTargetSpacing = 64;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.fPoSNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nTargetTimespan / nTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0;

        // SegWit not activated on Validity
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].min_activation_height = 0;

        // Taproot not activated on Validity
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0;

        // VALIDITY-PORT: Protocol times from validity-ref
        consensus.nProtocolV1RetargetingFixedTime = 1395631999;
        consensus.nProtocolV2Time = 1407053625;
        consensus.nProtocolV3Time = 1461851161;
        consensus.nProtocolV3_1Time = std::numeric_limits<int64_t>::max(); // Not used in Validity
        consensus.nLastPOWBlock = 20160;
        consensus.nStakeTimestampMask = 0xf; // 15
        consensus.nCoinbaseMaturity = 60;
        consensus.nStakeMinAge = 6 * 60 * 60; // 6 hours

        // VALIDITY-PORT: Dev fund parameters from validity-ref
        consensus.nDevFundBlockHeight = 1655000;
        consensus.nDevFundTime = 1535581967;
        consensus.sDevFundScript = "a914622b2274e97ebce0b25a5316ceca0af02120395087";
        consensus.nDevFundPaymentInterval = 10080;

        // VALIDITY-PORT: Pinned to the chainwork at block 5313600 (matches defaultAssumeValid).
        // Validity is PoS with ~2^71 cumulative work at tip, far below Bitcoin mainnet's ~2^98,
        // so we cannot reuse Bitcoin Core's default. Update both values together on each release.
        consensus.nMinimumChainWork = uint256S("0x00000000000000000000000000000000000000000000006cab41535cadf6cb06"); // chainwork at block 5313600
        consensus.defaultAssumeValid = uint256S("0xc1b0258f6209ecd4dcdfc4f9d8b35a3fe0a80de02a320929432f038c973cf78f"); // block 5313600

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        // VALIDITY-PORT: Magic bytes from validity-ref
        pchMessageStart[0] = 0x2a;
        pchMessageStart[1] = 0x7c;
        pchMessageStart[2] = 0xcb;
        pchMessageStart[3] = 0xab;
        nDefaultPort = 27913;
        m_assumed_blockchain_size = 20;

        // VALIDITY-PORT: Genesis block from validity-ref
        genesis = CreateGenesisBlock(1431857735, 109996, 0x1e0fffff, 1, 0);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x000000770c6aea829bb1ace7b06497f71799a6358e0e292740c4f9443a17bfb6"));
        assert(genesis.hashMerkleRoot == uint256S("0xa02b3388d9e8529bb0136ed3e1b823e808cbe128050e5986bf2a5a0d2c71a826"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as an addrfetch if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        // VALIDITY-PORT: No DNS seeds in original Validity; uses fixed seeds
        // vSeeds.emplace_back("...");

        // VALIDITY-PORT: Base58 prefixes from validity-ref
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,76);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,121);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x73, 0xAA, 0xA1};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x73, 0x44, 0x77};

        bech32_hrp = "validity";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // VALIDITY-PORT: Checkpoints from validity-ref/src/chainparams.cpp
        checkpointData = {
            {
                {      0, uint256S("0x000000770c6aea829bb1ace7b06497f71799a6358e0e292740c4f9443a17bfb6")},
                {   2879, uint256S("0x675ebb9f0f934c35bbf1955565482601ef68977b2fef8248689626004fcbce2d")}, // last pow
                {   2880, uint256S("0x3818386bc9fced59f21d5a9d1eab8a532c85efecb831eceee995ae1a7dc90419")}, // first pos
                { 100000, uint256S("0x97b8764441d1d0eff98e834cc743ec4cd6779d53f149dec9403c9758370abd8c")},
                { 200000, uint256S("0x98b0bcf0218293549a30d938a1d641b6bc6b498361cb432e3fafd6935c592846")},
                { 300000, uint256S("0xdcb25c38e2731a68d5bfa16b00b8479fed57c6a352cfd94d8e0dfdd690ac9ea2")},
                { 440000, uint256S("0x88914855018cf647d1b20162eed2dbc7cdf69757f8216868c1b55687aafe1a75")},
                { 500000, uint256S("0x41950e8d38381ce56fc0fd9eca76e73b49ce256c01ab2521897c5c6285b39c8d")},
                { 600000, uint256S("0x23141467be3b6fe23419c8a9fbcd4df431b4b1abcd43a1fa689e7b6d137e89ef")},
                { 700000, uint256S("0x6ceebeff2760988638367e5e91a9061bdc9623db1dceeaa7cef1eb8da88ec2bd")},
                { 800000, uint256S("0xbce932a50e264133d5456f1a8076552f5186f3f6ff46310f4c769ae83c4a1723")},
                { 900000, uint256S("0x5c58c0b472ab44dbde75c31e549615fc00feea3a93844d8c78837b2e85245c4e")},
                {1000000, uint256S("0x6950e1ca2bef4adfb963df7edaf92cac8127d3445554d23738bd88c135ce59e4")},
                {1200000, uint256S("0x619ffe5e099a182afaf5c37b0c73e60fd858acb2231ee29d95f4a223ba6e429a")},
                {1400000, uint256S("0x77d89a7da8d763c231ce82d8ea985648933446e952aadc19d8488c6b1813b1d7")},
                {1600000, uint256S("0xff5404d458f342ce6bee860e7c03ec847943da161cdb9914c23d5401d1744908")},
                {1800000, uint256S("0x8cb0ad88427f7f98b66d1018bcba9e78a873c89fcdefa4c87c33e56aeaba2dde")},
                {2000000, uint256S("0x6f5ffb06006b6724caf38de9fa67c8c98c3cd87ab0aca9a84cb983dc858be901")},
                {2200000, uint256S("0x231e74358bb0b0dc6b1261d16935d45a10982bf66c0c7ab208f02e28b9b6ae61")},
                {2400000, uint256S("0xb670ba23f814d23ec129999b26bb734011c06f51cf5887ae871a9500fd7158ef")},
                {2600000, uint256S("0x3f9e88853f237ecaad98b4f117cdae00e111ecc7aae2191a7a8ae4b4e2b43001")},
                {2800000, uint256S("0x0c565de4d82c333d827cb1058193dd8d4027d390deeb575b471783191a219122")},
                {3000001, uint256S("0x2102410c9753aaef958d22ff18042128850054126b2f99875cfa3221528521de")},
                {3200000, uint256S("0x16577739ce6854c0a4a7fa0149d7d60950930dcfd043d01053fcad5d4ea0b7b2")},
                {3600000, uint256S("0x81c8b14a19ca87782a0f732573ff37fb32ec1ac880b0d7f5f1f6f1679c822149")},
                {4000000, uint256S("0xfece0003fa9af506cb22c898baefd54368f43f00daa56447f65b9c0951655acb")},
                {4400000, uint256S("0x74fa0a981c659a95d676050e22440a4cd3fd893765c5397b6de47de0919b2b61")},
                {4800000, uint256S("0xc7961cc4fa6a1cf29dc5f71d2dee0d88f700181909a99c057b4250307d284562")},
                {5200000, uint256S("0x2e422e728c29ff37d0d29c78236eb229ca2c9d9dd239e85c86b0be8872afed06")},
                {5313600, uint256S("0xc1b0258f6209ecd4dcdfc4f9d8b35a3fe0a80de02a320929432f038c973cf78f")},
            }
        };

        m_assumeutxo_data = {
            // TODO to be specified in a future patch.
        };

        chainTxData = ChainTxData{
            // VALIDITY-PORT: From validity-ref checkpointData
            .nTime    = 1753586032,
            .nTxCount = 10736810,
            .dTxRate  = 10.0,
        };

        // VALIDITY-PORT: Dev fund address (p2sh multisig)
        vDevFundAddress = { "QVZ419DruuEQYxbCgvF6vNwYTJizbhC9qw" };
    }
};

/**
 * Testnet (v1): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nMaxReorganizationDepth = 500;
        consensus.CSVHeight = std::numeric_limits<int>::max();
        consensus.SegwitHeight = std::numeric_limits<int>::max();
        consensus.MinBIP9WarningHeight = std::numeric_limits<int>::max();
        // VALIDITY-PORT: Testnet limits from validity-ref
        consensus.powLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV2 = uint256S("000000000000ffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nTargetTimespan = 16 * 60; // 16 mins
        consensus.nTargetTimespanNEW = 16 * 60; // same as nTargetTimespan for testnet/regtest
        consensus.nTargetSpacingV1 = 60;
        consensus.nTargetSpacing = 64;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.fPoSNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0;

        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].min_activation_height = 0;

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0;

        // VALIDITY-PORT: Protocol times from validity-ref testnet
        consensus.nProtocolV1RetargetingFixedTime = 1395631999;
        consensus.nProtocolV2Time = 1407053625;
        consensus.nProtocolV3Time = 1444028400;
        consensus.nProtocolV3_1Time = std::numeric_limits<int64_t>::max();
        consensus.nLastPOWBlock = 0x7fffffff;
        consensus.nStakeTimestampMask = 0xf;
        consensus.nCoinbaseMaturity = 10;
        consensus.nStakeMinAge = 8 * 60 * 60; // 8 hours

        consensus.nMinimumChainWork = uint256S("0x00");
        consensus.defaultAssumeValid = uint256S("0x00");

        // VALIDITY-PORT: Testnet message start from validity-ref
        pchMessageStart[0] = 0xcd;
        pchMessageStart[1] = 0xf2;
        pchMessageStart[2] = 0xc0;
        pchMessageStart[3] = 0xef;
        nDefaultPort = 25714;
        m_assumed_blockchain_size = 5;

        // VALIDITY-PORT: Use permissive nBits for testnet genesis since pszTimestamp changed
        genesis = CreateGenesisBlock(1393221600, 216178, 0x207fffff, 1, 0);
        consensus.hashGenesisBlock = genesis.GetHash();

        vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tval";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        // VALIDITY-PORT: Testnet checkpoint from validity-ref
        checkpointData = {
            {
                { 90235, uint256S("0x567898e79184dc2f7dc3a661f794f28566e4b856d70180914f7371b1b3cc82d8")},
            }
        };

        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            .nTime    = 1549558800,
            .nTxCount = 179080,
            .dTxRate  = 2.0,
        };

        vDevFundAddress = {};
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vSeeds.clear();

        if (!options.challenge) {
            bin = ParseHex("512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae");
 
            /*
            vSeeds.emplace_back("seed.signet.bitcoin.sprovoost.nl.");

            // Hardcoded nodes can be removed once there are more DNS seeds
            vSeeds.emplace_back("178.128.221.177");
            vSeeds.emplace_back("v7ajjeirttkbnt32wpy3c6w3emwnfr3fkla7hpxcfokr3ysd3kqtzmqd.onion:38333");
            */

            vSeeds.clear();

            consensus.nMinimumChainWork = uint256S("0x00");
            consensus.defaultAssumeValid = uint256S("0x00");
            m_assumed_blockchain_size = 1;
            chainTxData = ChainTxData{
                // Data from RPC: getchaintxstats 4096 000000187d4440e5bff91488b700a140441e089a8aaea707414982460edbfe54
                .nTime    = 0,
                .nTxCount = 0,
                .dTxRate  = 0.0,
            };
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogPrintf("Signet with challenge %s\n", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nMaxReorganizationDepth = 500;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nTargetTimespan = 16 * 60; // 16 mins
        consensus.nTargetTimespanNEW = 16 * 60; // same as nTargetTimespan for testnet/regtest
        consensus.nTargetSpacingV1 = 64;
        consensus.nTargetSpacing = 64;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.fPoSNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 12000; // 80% of 15000
        consensus.nMinerConfirmationWindow = 15000; // nTargetTimespan / nTargetSpacing * 1000
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV2 = uint256S("000000000000ffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Activation of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        consensus.nProtocolV1RetargetingFixedTime = 1707168541;
        consensus.nProtocolV2Time = 1707168542;
        consensus.nProtocolV3Time = 1707168543;
        consensus.nProtocolV3_1Time = 1707168544;
        consensus.nLastPOWBlock = 0x7fffffff;
        consensus.nStakeTimestampMask = 0xf;
        consensus.nCoinbaseMaturity = 10;

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        std::copy_n(hash.begin(), 4, pchMessageStart.begin());

        nDefaultPort = 45714;

        consensus.nStakeMinAge = 1 * 60 * 60;

        // VALIDITY-PORT: Use permissive nBits for signet genesis since pszTimestamp changed
        genesis = CreateGenesisBlock(1393221600, 216178, 0x207fffff, 1, 0);
        consensus.hashGenesisBlock = genesis.GetHash();

        vFixedSeeds.clear();

        m_assumeutxo_data = {};

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "tval";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        vDevFundAddress = {};
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nMaxReorganizationDepth = 50;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.posLimitV2 = uint256S("000000000000ffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nTargetTimespan = 16 * 60; // 16 mins
        consensus.nTargetTimespanNEW = 16 * 60; // same as nTargetTimespan for testnet/regtest
        consensus.nTargetSpacingV1 = 64;
        consensus.nTargetSpacing = 64;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.fPoSNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 120; // 80% for regtest
        consensus.nMinerConfirmationWindow = 150; // Faster than normal for regtest (150 instead of 15000)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        consensus.nProtocolV1RetargetingFixedTime = 1395631999;
        consensus.nProtocolV2Time = 1407053625;
        consensus.nProtocolV3Time = 1444028400;
        consensus.nProtocolV3_1Time = std::numeric_limits<int64_t>::max();
        consensus.nLastPOWBlock = 1000;
        consensus.nStakeTimestampMask = 0xf;
        consensus.nCoinbaseMaturity = 10;
        consensus.nStakeMinAge = 1 * 60 * 60; // 1 hour

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0x70;
        pchMessageStart[1] = 0x35;
        pchMessageStart[2] = 0x22;
        pchMessageStart[3] = 0x06;
        nDefaultPort = 25714;
        m_assumed_blockchain_size = 0;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            /*
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            */
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }

        // VALIDITY-PORT: Use permissive nBits for regtest genesis since pszTimestamp changed
        genesis = CreateGenesisBlock(1393221600, 216178, 0x207fffff, 1, 0);
        consensus.hashGenesisBlock = genesis.GetHash();

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {
                {0, consensus.hashGenesisBlock},
            }
        };

        m_assumeutxo_data = {};

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "valrt";

        vDevFundAddress = {};
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}
