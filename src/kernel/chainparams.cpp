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
#include <nlohmann/json.hpp>

using namespace util::hex_literals;
using json = nlohmann::json;

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
    bool foundGenesisBlock = false;
    bool foundMerkleRoot = false;

    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line.find("//") == 0) continue;

        if (line.find("consensus.hashGenesisBlock") != std::string::npos) {
            std::string hashStr = line.substr(line.find("uint256{") + 8, 64);
            std::vector<unsigned char> hashBytes = ParseHex(hashStr);
            if (hashBytes.size() == 32) {
                hashGenesisBlock = uint256(hashBytes);
                foundGenesisBlock = true;
            }
        } else if (line.find("genesis.hashMerkleRoot") != std::string::npos) {
            std::string merkleStr = line.substr(line.find("uint256{") + 8, 64);
            std::vector<unsigned char> merkleBytes = ParseHex(merkleStr);
            if (merkleBytes.size() == 32) {
                hashMerkleRoot = uint256(merkleBytes);
                foundMerkleRoot = true;
            }
        } else if (line.find("CreateGenesisBlock") != std::string::npos) {
            // Extract parameters from CreateGenesisBlock
            std::string params = line.substr(line.find("(") + 1);
            std::istringstream iss(params);
            std::string param;
            int paramIndex = 0;
            
            while (std::getline(iss, param, ',')) {
                param = param.substr(0, param.find("//")); // Remove comments
                param.erase(0, param.find_first_not_of(" \t")); // Trim leading whitespace
                param.erase(param.find_last_not_of(" \t") + 1); // Trim trailing whitespace
                
                try {
                    switch (paramIndex) {
                        case 2: // Timestamp
                            nTime = std::stoul(param);
                            break;
                        case 3: // Nonce
                            nNonce = std::stoul(param);
                            break;
                        case 4: // nBits
                            if (param.find("0x") != std::string::npos) {
                                nBits = std::stoul(param.substr(2), nullptr, 16);
                            }
                            break;
                        case 5: // nVersion
                            nVersion = std::stoi(param);
                            break;
                        case 6: // genesisReward
                            if (param.find("COIN") != std::string::npos) {
                                genesisReward = 50 * COIN;
                            }
                            break;
                    }
                } catch (const std::exception& e) {
                    LogPrintf("Warning: Failed to parse parameter %d: %s\n", paramIndex, e.what());
                }
                paramIndex++;
            }
        }
    }

    if (foundGenesisBlock && foundMerkleRoot) {
        LogPrintf("Successfully loaded genesis block parameters from zia_genesis_output.txt\n");
        return true;
    }
    
    return false;
}

// New function to load genesis block parameters from mining_params.json
static bool LoadMiningParameters(uint256& hashGenesisBlock, uint256& hashMerkleRoot, uint32_t& nTime, uint32_t& nNonce, uint32_t& nBits)
{
    std::filesystem::path miningPath = std::filesystem::path("mining_params.json");
    if (!std::filesystem::exists(miningPath)) {
        LogPrintf("Warning: mining_params.json not found. Using default genesis block.\n");
        return false;
    }

    try {
        std::ifstream file(miningPath);
        json mining_params = json::parse(file);

        // Load parameters from JSON
        std::string hashStr = mining_params["hash"].get<std::string>();
        std::string merkleStr = mining_params["merkle_root"].get<std::string>();
        nTime = mining_params["timestamp"].get<uint32_t>();
        nNonce = mining_params["nonce"].get<uint32_t>();
        std::string bitsStr = mining_params["n_bits"].get<std::string>();
        nBits = std::stoul(bitsStr, nullptr, 16);

        // Convert hex strings to uint256
        std::vector<unsigned char> hashBytes = ParseHex(hashStr);
        std::vector<unsigned char> merkleBytes = ParseHex(merkleStr);
        
        if (hashBytes.size() == 32 && merkleBytes.size() == 32) {
            hashGenesisBlock = uint256(hashBytes);
            hashMerkleRoot = uint256(merkleBytes);
            LogPrintf("Loaded mining parameters from mining_params.json\n");
            return true;
        }
    } catch (const std::exception& e) {
        LogPrintf("Warning: Failed to parse mining_params.json: %s\n", e.what());
    }
    return false;
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
        const char* genesis_msg = "The beginning of ZiaCoin - 2025-05-24";  // This will be overridden if loading from file
        const CScript genesis_script = CScript() << ParseHex("04a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b0") << OP_CHECKSIG;

        // Try to load mining parameters first
        uint256 loadedHashGenesisBlock;
        uint256 loadedHashMerkleRoot;
        uint32_t loadedNTime = 1748190366;  // Default timestamp
        uint32_t loadedNNonce = 16389;      // Default nonce
        uint32_t loadedNBits = 0x207fffff;  // Default nBits
        int32_t loadedNVersion = 1;         // Default version
        CAmount loadedGenesisReward = 50 * COIN;

        CBlock genesis;
        bool loadedFromMining = LoadMiningParameters(loadedHashGenesisBlock, loadedHashMerkleRoot, loadedNTime, loadedNNonce, loadedNBits);
        bool loadedFromFile = false;
        
        if (!loadedFromMining) {
            // If no mining parameters, try loading from genesis output file
            loadedFromFile = LoadGenesisFromFile(genesis, loadedHashGenesisBlock, loadedHashMerkleRoot, loadedNTime, loadedNNonce, loadedNBits, loadedNVersion, loadedGenesisReward);
        }
        
        if (loadedFromMining || loadedFromFile) {
            // If we loaded from either source, use those parameters
            LogPrintf("Using genesis block parameters from %s\n", loadedFromMining ? "mining_params.json" : "zia_genesis_output.txt");
            genesis = CreateGenesisBlock(genesis_msg, genesis_script, loadedNTime, loadedNNonce, loadedNBits, loadedNVersion, loadedGenesisReward);
            consensus.hashGenesisBlock = loadedHashGenesisBlock;
            genesis.hashMerkleRoot = loadedHashMerkleRoot;
        } else {
            // If no parameters found, create default genesis block
            LogPrintf("No genesis parameters found, using default parameters\n");
            genesis = CreateGenesisBlock(genesis_msg, genesis_script, loadedNTime, loadedNNonce, loadedNBits, loadedNVersion, loadedGenesisReward);
            consensus.hashGenesisBlock = genesis.GetHash();
        }

        // Print debug information to terminal
        std::string genesisHashStr = consensus.hashGenesisBlock.ToString();
        std::string merkleRootStr = genesis.hashMerkleRoot.ToString();
        LogPrintf("Generated genesis block hash: %s\n", genesisHashStr);
        LogPrintf("Genesis block merkle root: %s\n", merkleRootStr);

        // Only assert if we have parameters from either source
        if (loadedFromMining || loadedFromFile) {
            assert(consensus.hashGenesisBlock == loadedHashGenesisBlock);
            assert(genesis.hashMerkleRoot == loadedHashMerkleRoot);
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