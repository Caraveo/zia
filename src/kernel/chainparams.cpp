// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-present The ZiaCoin Core developers
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

using namespace util::hex_literals;

// Workaround MSVC bug triggering C7595 when calling consteval constructors in
// initializer lists.
// https://developercommunity.visualstudio.com/t/Bogus-C7595-error-on-valid-C20-code/10906093
#if defined(_MSC_VER)
auto consteval_ctor(auto&& input) { return input; }
#else
#define consteval_ctor(input) (input)
#endif

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

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
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "03/May/2024 ZiaCoin Network Launch";
    const CScript genesisOutputScript = CScript() << ParseHex("04a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b") << OP_CHECKSIG;
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
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.script_flag_exceptions.clear();
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1815;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        // ZiaCoin message start bytes
        pchMessageStart[0] = 0x5a; // Z
        pchMessageStart[1] = 0x69; // i
        pchMessageStart[2] = 0x61; // a
        pchMessageStart[3] = 0x43; // C
        nDefaultPort = 8334;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        // Genesis block parameters for ZiaCoin
        const char* genesis_msg = "The beginning of ZiaCoin - 2025-05-24";
        const CScript genesis_script = CScript() << ParseHex("04a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b0") << OP_CHECKSIG;
        
        genesis = CreateGenesisBlock("The beginning of ZiaCoin - 2025-05-24", 1748190366, 16389, 0x207fffff, 1, 50 * COIN);
                genesis_msg,
                genesis_script,
                1748190366, // Timestamp: 2025-05-24
                16389,      // Nonce
                0x207fffff, // nBits (easy mining)
                1,          // nVersion
                50 * COIN); // genesisReward
        
        consensus.hashGenesisBlock = genesis.GetHash();
        
        // Print debug information to terminal
        fprintf(stderr, "Generated genesis block hash: %s\n", consensus.hashGenesisBlock.ToString().c_str());
        fprintf(stderr, "Expected genesis block hash: 00009cafe705f9eab60c30fc7e1c90c43741551efcf34f4a1b715a8171a076dd\n");
        fprintf(stderr, "Genesis block merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());
        
        LogPrintf("Generated genesis block hash: %s\n", consensus.hashGenesisBlock.ToString());
        LogPrintf("Expected genesis block hash: 00009cafe705f9eab60c30fc7e1c90c43741551efcf34f4a1b715a8171a076dd\n");
        LogPrintf("Genesis block merkle root: %s\n", genesis.hashMerkleRoot.ToString());
        
        assert(consensus.hashGenesisBlock == uint256{"00009cafe705f9eab60c30fc7e1c90c43741551efcf34f4a1b715a8171a076dd"});
        assert(genesis.hashMerkleRoot == uint256{"9f53a7cc67254bce29a7cfe281ce26a22dbfa469f35218f541c3aed6274bdb11"});

        // Clear Bitcoin's seed nodes
        vFixedSeeds.clear();
        vSeeds.clear();

        // ZiaCoin address prefixes
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 65);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 125);
        base58Prefixes[SECRET_KEY]     = std::vector<unsigned char>(1, 212);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "zia";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        chainTxData = ChainTxData{
            0,
            0,
            0
        };
    }
};

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::optional<ChainType> GetNetworkForMagic(const MessageStartChars& message)
{
    const auto mainnet_msg = CChainParams::Main()->MessageStart();
    if (std::ranges::equal(message, mainnet_msg)) {
        return ChainType::MAIN;
    }
    return std::nullopt;
}
