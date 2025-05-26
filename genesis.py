#!/usr/bin/env python3

import hashlib
import time
import struct
import os
import platform
from pathlib import Path
import binascii
import sys
from datetime import datetime
import re
import shutil
import json

# ======== Custom Network Settings ========
network_name   = "ZiaCoin Mainnet"
bech32_prefix  = "zia"
current_time = int(time.time())
psz_timestamp  = f"The beginning of ZiaCoin - {datetime.fromtimestamp(current_time).strftime('%Y-%m-%d')}"
pubkey         = "04a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b0"
n_time         = current_time  # Current timestamp
FAST_MINE = True  # Set True for fast mining, False for real difficulty

if FAST_MINE:
    n_bits = 0x207fffff  # very low difficulty, easy to mine
else:
    n_bits = 0x1d00ffff  # Bitcoin mainnet difficulty
max_nonce      = 0x100000000
reward         = "00f2052a01000000"  # 50 ZIA in satoshis

# Default ports by network
default_ports = {
    "mainnet": 9888,
    "testnet": 19888,
    "testnet4": 18888,
    "signet": 17888,
    "regtest": 15888,
}

# ======== Block Construction ========
def merkle_root(tx_hashes):
    if not tx_hashes:
        return b""
    while len(tx_hashes) > 1:
        if len(tx_hashes) % 2 != 0:
            tx_hashes.append(tx_hashes[-1])
        new_level = []
        for i in range(0, len(tx_hashes), 2):
            new_hash = hashlib.sha256(hashlib.sha256(tx_hashes[i] + tx_hashes[i+1]).digest()).digest()
            new_level.append(new_hash)
        tx_hashes = new_level
    return tx_hashes[0]

def create_coinbase_tx():
    global psz_timestamp
    
    script_sig_bytes = psz_timestamp.encode('ascii')
    script_sig = (struct.pack("B", len(script_sig_bytes)) + script_sig_bytes).hex()
    
    if len(pubkey) != 130 or not all(c in '0123456789abcdefABCDEF' for c in pubkey):
        raise ValueError(f"Invalid pubkey: must be 130 hex characters, got {len(pubkey)}. Value: {pubkey}")
    
    output_script = "41" + pubkey + "ac"
    reward_le = struct.pack("<Q", 50 * 100000000).hex()
    
    tx = (
        "01000000" + "01" +
        "00" * 32 +
        "ffffffff" +
        f"{len(script_sig)//2:02x}" + script_sig +
        "ffffffff" + "01" +
        reward_le +
        f"{len(output_script)//2:02x}" + output_script +
        "00000000"
    )
    
    print("\nCoinbase Transaction Details:")
    print(f"Timestamp: {psz_timestamp}")
    print(f"Script Sig Bytes: {len(script_sig_bytes)}")
    print(f"Script Sig Length: {len(script_sig)//2:02x}")
    print(f"Script Sig: {script_sig}")
    print(f"Output Script: {output_script}")
    print(f"Reward (LE): {reward_le}")
    print(f"Full TX: {tx}")
    
    tx_bytes = bytes.fromhex(tx)
    tx_hash = hashlib.sha256(hashlib.sha256(tx_bytes).digest()).digest()
    print(f"TX Hash: {tx_hash.hex()}")
    
    return tx_bytes

def calculate_genesis_hash(merkle_root, n_time, n_bits, n_nonce):
    header = (
        struct.pack("<L", 1) +
        bytes.fromhex("00" * 32) +
        merkle_root +
        struct.pack("<L", n_time) +
        struct.pack("<L", n_bits) +
        struct.pack("<L", n_nonce)
    )
    hash1 = hashlib.sha256(header).digest()
    hash2 = hashlib.sha256(hash1).digest()
    return hash2[::-1].hex()

# ======== Save Mining Parameters ========
def save_mining_parameters(hash_hex, nonce, merkle_hex, timestamp, n_bits):
    mining_params = {
        'hash': hash_hex,
        'nonce': nonce,
        'merkle_root': merkle_hex,
        'timestamp': timestamp,
        'n_bits': hex(n_bits),
        'date': datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S')
    }
    
    with open("mining_params.json", "w") as f:
        json.dump(mining_params, f, indent=4)
    print(f"\n📄 Mining parameters saved to mining_params.json")

# ======== Mining Loop ========
print("🔨 Searching for genesis block...")
print(f"Timestamp: {n_time} ({datetime.fromtimestamp(n_time).strftime('%Y-%m-%d %H:%M:%S')})")

coinbase_tx = create_coinbase_tx()
tx_hash = hashlib.sha256(hashlib.sha256(coinbase_tx).digest()).digest()
merkle = merkle_root([tx_hash])
merkle_hex = merkle.hex()
print(f"\nGenerated Merkle Root: {merkle_hex}")
print(f"[DEBUG] Merkle from fresh tx: {merkle[::-1].hex()}")

genesis_nonce = 0
hash_hex = ""

# Try current time and a few minutes before
timestamps = [current_time]
for i in range(1, 6):
    timestamps.append(current_time - (i * 600))  # Try 10 minutes before each time

for timestamp in timestamps:
    print(f"\nTrying timestamp: {timestamp} ({datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S')})")
    n_time = timestamp
    
    for nonce in range(max_nonce):
        hash_hex = calculate_genesis_hash(merkle, n_time, n_bits, nonce)
        if FAST_MINE:
            if hash_hex.startswith("0000"):
                print(f"✅ Found easy valid hash: {hash_hex} at nonce {nonce}")
                genesis_nonce = nonce
                break
        else:
            if hash_hex.startswith("000000"):
                print(f"✅ Found valid hash: {hash_hex} at nonce {nonce}")
                genesis_nonce = nonce
                break
        if nonce % 1000000 == 0:
            print(f"Trying nonce: {nonce} (current hash: {hash_hex})")
    
    if genesis_nonce != 0:
        break

if genesis_nonce == 0:
    print("❌ Could not find valid hash. Try adjusting timestamp or other parameters.")
    sys.exit(1)

print(f"\nMining completed. Used difficulty: {hex(n_bits)}")
print(f"Final hash: {hash_hex}")
print(f"Final nonce: {genesis_nonce}")
print(f"Final merkle root: {merkle_hex}")
print(f"Final timestamp: {n_time} ({datetime.fromtimestamp(n_time).strftime('%Y-%m-%d %H:%M:%S')})")

# Save the mining parameters
save_mining_parameters(hash_hex, genesis_nonce, merkle_hex, n_time, n_bits)

# ======== Output Final Config ========
print("\n=== ZiaCoin Genesis Block (C++ formatted) ===")
print(f'// Generated for {network_name}')
print('// Genesis block parameters for ZiaCoin')
print(f'const char* genesis_msg = "{psz_timestamp}";')
print(f'const CScript genesis_script = CScript() << ParseHex("{pubkey}") << OP_CHECKSIG;')
print()
print('CBlock genesis = CreateGenesisBlock(')
print(f'    genesis_msg,')
print(f'    genesis_script,')
print(f'    {n_time},    // Timestamp: {datetime.fromtimestamp(n_time).strftime("%Y-%m-%d")}')
print(f'    {genesis_nonce},   // Nonce')
print(f'    {hex(n_bits)}, // nBits')
print(f'    1,          // nVersion')
print(f'    50 * COIN   // genesisReward')
print(');')
print()
print(f'consensus.hashGenesisBlock = genesis.GetHash();')
print(f'assert(consensus.hashGenesisBlock == uint256{{"{hash_hex}"}});')
print(f'assert(genesis.hashMerkleRoot == uint256{{"{merkle_hex}"}});')
print(f'bech32_hrp = "{bech32_prefix}";')
print()

print("=== Default Ports for chainparamsbase.cpp ===")
for net, port in default_ports.items():
    print(f'case ChainType::{net.upper()}: return std::make_unique<CBaseChainParams>("{net if net != "mainnet" else ""}", {port});')

# ======== Save to file ========
with open("zia_genesis_output.txt", "w") as f:
    f.write("// ZiaCoin Genesis C++ Configuration\n")
    f.write('// Genesis block parameters for ZiaCoin\n')
    f.write(f'const char* genesis_msg = "{psz_timestamp}";\n')
    f.write(f'const CScript genesis_script = CScript() << ParseHex("{pubkey}") << OP_CHECKSIG;\n\n')
    f.write('CBlock genesis = CreateGenesisBlock(\n')
    f.write(f'    genesis_msg,\n')
    f.write(f'    genesis_script,\n')
    f.write(f'    {n_time},    // Timestamp: {datetime.fromtimestamp(n_time).strftime("%Y-%m-%d")}\n')
    f.write(f'    {genesis_nonce},   // Nonce\n')
    f.write(f'    {hex(n_bits)}, // nBits\n')
    f.write(f'    1,          // nVersion\n')
    f.write(f'    50 * COIN   // genesisReward\n')
    f.write(');\n\n')
    f.write(f'consensus.hashGenesisBlock = genesis.GetHash();\n')
    f.write(f'assert(consensus.hashGenesisBlock == uint256{{"{hash_hex}"}});\n')
    f.write(f'assert(genesis.hashMerkleRoot == uint256{{"{merkle_hex}"}});\n')
    f.write(f'bech32_hrp = "{bech32_prefix}";\n\n')
    f.write("// Base58 prefixes\n")
    f.write('base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 65);\n')
    f.write('base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 125);\n')
    f.write('base58Prefixes[SECRET_KEY]     = std::vector<unsigned char>(1, 212);\n\n')
    f.write("// Default Ports\n")
    for net, port in default_ports.items():
        f.write(f'case ChainType::{net.upper()}: return std::make_unique<CBaseChainParams>("{net if net != "mainnet" else ""}", {port});\n')

print("\n📄 C++ config lines written to zia_genesis_output.txt")

# ======== Dynamic Config File Generation ========
def get_default_datadir():
    system = platform.system()
    if system == "Windows":
        return Path(os.environ["APPDATA"]) / "ZiaCoin"
    elif system == "Darwin":
        return Path.home() / "Library/Application Support" / "ZiaCoin"
    else:  # Linux
        return Path.home() / ".ziacoin"

def write_default_config(datadir):
    config_path = datadir / "ziacoin.conf"
    if config_path.exists():
        print(f"📄 Config already exists at {config_path}, skipping.")
        return

    config = f"""# ZiaCoin Configuration File

# === Network Settings ===
# testnet=1
# regtest=1
listen=1
maxconnections=40
maxuploadtarget=5000

# === RPC Settings ===
server=1
rpcuser=ziauser
rpcpassword=ziapass
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
rpcport=38832

# === Wallet Settings ===
wallet=wallet.dat
disablewallet=0
"""
    datadir.mkdir(parents=True, exist_ok=True)
    with open(config_path, "w") as f:
        f.write(config)
    print(f"✅ Wrote default config to {config_path}")

# ======== Run config writer ========
default_datadir = get_default_datadir()
write_default_config(default_datadir)

def uint256_from_str(s):
    r = 0
    t = struct.unpack("<IIIIIIII", s[:32])
    for i in range(8):
        r += t[i] << (i * 32)
    return r

def uint256_from_compact(c):
    nbytes = (c >> 24) & 0xFF
    v = (c & 0xFFFFFF) << (8 * (nbytes - 3))
    return v

def generate_genesis_block(pszTimestamp, pubkey, nTime, nNonce, nBits, nVersion, genesisReward):
    merkleRoot = hashlib.sha256(hashlib.sha256(pszTimestamp).digest()).digest()
    
    # Genesis block header
    blockHeader = struct.pack("<I", nVersion)
    blockHeader += b"\x00" * 32  # prev_block
    blockHeader += merkleRoot
    blockHeader += struct.pack("<I", nTime)
    blockHeader += struct.pack("<I", nBits)
    blockHeader += struct.pack("<I", nNonce)
    
    # Calculate block hash
    blockHash = hashlib.sha256(hashlib.sha256(blockHeader).digest()).digest()
    
    return {
        'hash': binascii.hexlify(blockHash).decode('utf-8'),
        'merkle_root': binascii.hexlify(merkleRoot).decode('utf-8'),
        'timestamp': nTime,
        'nonce': nNonce,
        'bits': nBits,
        'version': nVersion
    }

def main():
    # Genesis block parameters
    pszTimestamp = "The beginning of ZiaCoin - " + datetime.now().strftime("%Y-%m-%d")
    nTime = int(time.time())
    nNonce = 0
    nBits = 0x1d00ffff
    nVersion = 1
    genesisReward = 50 * 100000000  # 50 ZIA in satoshis

    print("Generating genesis block...")
    print("Timestamp:", pszTimestamp)
    print("Time:", nTime)
    print("Nonce:", nNonce)
    print("Bits:", hex(nBits))
    print("Version:", nVersion)
    print("Reward:", genesisReward)

    result = generate_genesis_block(
        pszTimestamp.encode('utf-8'),
        pubkey,
        nTime,
        nNonce,
        nBits,
        nVersion,
        genesisReward
    )

    print("\nGenesis Block Generated:")
    print("Hash:", result['hash'])
    print("Merkle Root:", result['merkle_root'])
    print("\nC++ Code:")
    print(f"""
    // Genesis block parameters for ZiaCoin
    const char* genesis_msg = "{pszTimestamp}";
    const CScript genesis_script = CScript() << ParseHex("{pubkey}") << OP_CHECKSIG;
    
    CBlock genesis = CreateGenesisBlock(genesis_msg,
            genesis_script,
            {nTime},    // Timestamp
            {nNonce},   // Nonce
            0x{nBits:x}, // nBits
            {nVersion}, // nVersion
            {genesisReward} * COIN); // genesisReward
    
    consensus.hashGenesisBlock = genesis.GetHash();
    assert(consensus.hashGenesisBlock == uint256{{"{result['hash']}"}});
    assert(genesis.hashMerkleRoot == uint256{{"{result['merkle_root']}"}});
    """)

    shutil.copy("zia_genesis_output.txt", "src/zia_genesis_output.txt")

if __name__ == '__main__':
    main()