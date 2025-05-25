#!/usr/bin/env python3

import hashlib
import time
import struct

# Customize this!
psz_timestamp = "The beginning of ZiaCoin — 2025-05-24"
pubkey = "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f"
n_time = int(time.time())
n_bits = 0x1d00ffff  # Difficulty
max_nonce = 0x100000000

# Merkle root (just coinbase tx)
def merkle_root(tx_hashes):
    if not tx_hashes:
        return ''
    while len(tx_hashes) > 1:
        if len(tx_hashes) % 2 != 0:
            tx_hashes.append(tx_hashes[-1])
        new_level = []
        for i in range(0, len(tx_hashes), 2):
            new_hash = hashlib.sha256(hashlib.sha256(tx_hashes[i] + tx_hashes[i+1]).digest()).digest()
            new_level.append(new_hash)
        tx_hashes = new_level
    return tx_hashes[0]

def little_endian(hex_str):
    return bytes.fromhex(hex_str)[::-1]

def create_coinbase_tx():
    script_sig = (
        struct.pack("B", len(psz_timestamp)) +
        psz_timestamp.encode()
    ).hex()
    tx = (
        "01000000" +  # Version
        "01" +  # Inputs count
        "0" * 64 +  # Prev tx hash
        "ffffffff" +  # Index
        f"{len(script_sig)//2:02x}" + script_sig +
        "ffffffff" +  # Sequence
        "01" +  # Outputs count
        "00f2052a01000000" +  # 50 ZIA
        "43" + pubkey + "ac" +  # P2PK script
        "00000000"  # Locktime
    )
    return bytes.fromhex(tx)

def calculate_genesis_hash(merkle_root, n_time, n_bits, n_nonce):
    version = struct.pack("<L", 1)
    prev_block = bytes.fromhex("00" * 32)
    merkle = merkle_root
    time_bytes = struct.pack("<L", n_time)
    bits_bytes = struct.pack("<L", n_bits)
    nonce_bytes = struct.pack("<L", n_nonce)
    header = version + prev_block + merkle + time_bytes + bits_bytes + nonce_bytes
    hash_bin = hashlib.sha256(hashlib.sha256(header).digest()).digest()
    return hash_bin[::-1].hex()  # big-endian

print(f"🔨 Searching genesis block...")
coinbase_tx = create_coinbase_tx()
tx_hash = hashlib.sha256(hashlib.sha256(coinbase_tx).digest()).digest()
merkle = merkle_root([tx_hash])
best_hash = "f" * 64
genesis_nonce = 0
for nonce in range(max_nonce):
    hash_hex = calculate_genesis_hash(merkle, n_time, n_bits, nonce)
    if hash_hex < best_hash:
        best_hash = hash_hex
        genesis_nonce = nonce
    if hash_hex.startswith("000000"):
        print(f"\n✅ Found genesis hash!")
        break

print("\n=== ZiaCoin Genesis Block Parameters ===")
print(f'pszTimestamp = "{psz_timestamp}"')
print(f"nTime = {n_time}")
print(f"nNonce = {genesis_nonce}")
print(f"nBits = {hex(n_bits)}")
print(f"hashGenesisBlock = {best_hash}")
print(f"hashMerkleRoot = {merkle[::-1].hex()}")