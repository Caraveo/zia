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
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

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

// New function to load genesis block parameters from zia_genesis_output.txt if it exists
static bool LoadGenesisFromFile(CBlock& genesis, uint256& hashGenesisBlock, uint256& hashMerkleRoot, uint32_t& nTime, uint32_t& nNonce, uint32_t& nBits, int32_t& nVersion, CAmount& genesisReward)
{
    std::filesystem::path genesisPath = std::filesystem::path("src") / "zia_genesis_output.txt";
    if (!std::filesystem::exists(genesisPath)) {
        LogPrintf("Warning: zia_genesis_output.txt not found in src directory. Using default genesis block.\n");
        return false;
    }

    std::ifstream file(genesisPath);
    if (!file.is_open()) {
        LogPrintf("Warning: Failed to open zia_genesis_output.txt. Using default genesis block.\n");
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line.find("//") == 0) continue;

        if (line.find("consensus.hashGenesisBlock") != std::string::npos) {
            std::string hashStr = line.substr(line.find("uint256{") + 8, 64);
            std::vector<unsigned char> hashBytes = ParseHex(hashStr);
            if (hashBytes.size() == 32) {
                hashGenesisBlock = uint256(hashBytes);
            }
        } else if (line.find("genesis.hashMerkleRoot") != std::string::npos) {
            std::string merkleStr = line.substr(line.find("uint256{") + 8, 64);
            std::vector<unsigned char> merkleBytes = ParseHex(merkleStr);
            if (merkleBytes.size() == 32) {
                hashMerkleRoot = uint256(merkleBytes);
            }
        } else if (line.find("Timestamp:") != std::string::npos) {
            // Extract timestamp from comment
            size_t pos = line.find("// Timestamp:") + 13;
            while (pos < line.length() && std::isspace(line[pos])) pos++;
            std::string timeStr = line.substr(pos);
            try {
                nTime = std::stoul(timeStr);
            } catch (const std::exception& e) {
                LogPrintf("Warning: Failed to parse timestamp: %s\n", e.what());
            }
        } else if (line.find("Nonce") != std::string::npos) {
            // Extract nonce from CreateGenesisBlock parameters
            size_t pos = line.find(",");
            if (pos != std::string::npos) {
                pos++;
                while (pos < line.length() && std::isspace(line[pos])) pos++;
                std::string nonceStr = line.substr(pos);
                try {
                    nNonce = std::stoul(nonceStr);
                } catch (const std::exception& e) {
                    LogPrintf("Warning: Failed to parse nonce: %s\n", e.what());
                }
            }
        } else if (line.find("nBits") != std::string::npos) {
            // Extract nBits from hex value
            size_t pos = line.find("0x");
            if (pos != std::string::npos) {
                std::string bitsStr = line.substr(pos + 2);
                try {
                    nBits = std::stoul(bitsStr, nullptr, 16);
                } catch (const std::exception& e) {
                    LogPrintf("Warning: Failed to parse nBits: %s\n", e.what());
                }
            }
        } else if (line.find("nVersion") != std::string::npos) {
            // Extract version from CreateGenesisBlock parameters
            size_t pos = line.find(",");
            if (pos != std::string::npos) {
                pos++;
                while (pos < line.length() && std::isspace(line[pos])) pos++;
                std::string versionStr = line.substr(pos);
                try {
                    nVersion = std::stoi(versionStr);
                } catch (const std::exception& e) {
                    LogPrintf("Warning: Failed to parse version: %s\n", e.what());
                }
            }
        } else if (line.find("genesisReward") != std::string::npos) {
            genesisReward = 50 * COIN;
        }
    }

    LogPrintf("Loaded genesis block parameters from zia_genesis_output.txt\n");
    return true;
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
        nDefaultPort = 38832;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        // Genesis block parameters for ZiaCoin
        const char* genesis_msg = "The beginning of ZiaCoin - 2025-05-24";
        const CScript genesis_script = CScript() << ParseHex("04a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b0") << OP_CHECKSIG;

        // Try to load genesis block parameters from zia_genesis_output.txt first
        uint256 loadedHashGenesisBlock;
        uint256 loadedHashMerkleRoot;
        uint32_t loadedNTime = 1748190366;  // Default timestamp
        uint32_t loadedNNonce = 16389;      // Default nonce
        uint32_t loadedNBits = 0x207fffff;  // Default nBits
        int32_t loadedNVersion = 1;         // Default version
        CAmount loadedGenesisReward = 50 * COIN;

        CBlock genesis;
        bool loadedFromFile = LoadGenesisFromFile(genesis, loadedHashGenesisBlock, loadedHashMerkleRoot, loadedNTime, loadedNNonce, loadedNBits, loadedNVersion, loadedGenesisReward);
        
        if (loadedFromFile) {
            // If we loaded from file, use those parameters exactly as they are
            LogPrintf("Using genesis block parameters from zia_genesis_output.txt\n");
            genesis = CreateGenesisBlock(genesis_msg, genesis_script, loadedNTime, loadedNNonce, loadedNBits, loadedNVersion, loadedGenesisReward);
            consensus.hashGenesisBlock = loadedHashGenesisBlock;
            genesis.hashMerkleRoot = loadedHashMerkleRoot;
        } else {
            // If no file, create default genesis block
            LogPrintf("No genesis parameters file found, using default parameters\n");
            genesis = CreateGenesisBlock(genesis_msg, genesis_script, loadedNTime, loadedNNonce, loadedNBits, loadedNVersion, loadedGenesisReward);
            consensus.hashGenesisBlock = genesis.GetHash();
        }

        // Print debug information to terminal
        fprintf(stderr, "Generated genesis block hash: %s\n", consensus.hashGenesisBlock.ToString().c_str());
        fprintf(stderr, "Expected genesis block hash: 00009cafe705f9eab60c30fc7e1c90c43741551efcf34f4a1b715a8171a076dd\n");
        fprintf(stderr, "Genesis block merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        // Only assert if we're not regenerating genesis
        if (!std::filesystem::exists(std::filesystem::path("src") / "zia_genesis_output.txt")) {
            assert(consensus.hashGenesisBlock == uint256{"00009cafe705f9eab60c30fc7e1c90c43741551efcf34f4a1b715a8171a076dd"});
            assert(genesis.hashMerkleRoot == uint256{"9f53a7cc67254bce29a7cfe281ce26a22dbfa469f35218f541c3aed6274bdb11"});
        }

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