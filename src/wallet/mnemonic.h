#ifndef BITCOIN_WALLET_MNEMONIC_H
#define BITCOIN_WALLET_MNEMONIC_H

#include <string>
#include <vector>

namespace wallet {

/**
 * Generate a BIP39 mnemonic phrase from entropy.
 * @param entropy_size The size of the entropy in bits (must be a multiple of 32, typically 128, 160, 192, 224, or 256).
 * @return A mnemonic phrase as a string.
 */
std::string GenerateMnemonic(int entropy_size = 128);

/**
 * Validate a BIP39 mnemonic phrase.
 * @param mnemonic The mnemonic phrase to validate.
 * @return True if the mnemonic is valid, false otherwise.
 */
bool ValidateMnemonic(const std::string& mnemonic);

/**
 * Derive a BIP32 seed from a mnemonic phrase and optional passphrase.
 * @param mnemonic The mnemonic phrase.
 * @param passphrase An optional passphrase (default is empty).
 * @return The derived seed as a byte vector.
 */
std::vector<unsigned char> MnemonicToSeed(const std::string& mnemonic, const std::string& passphrase = "");

} // namespace wallet

#endif // BITCOIN_WALLET_MNEMONIC_H 