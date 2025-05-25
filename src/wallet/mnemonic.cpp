#include <wallet/mnemonic.h>
#include <random.h>
#include <util/strencodings.h>
#include <crypto/sha256.h>
#include <crypto/hmac_sha512.h>
#include <span.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>

extern "C" {
#include <openssl/evp.h>
#include <openssl/sha.h>
}

namespace wallet {

// Load wordlist from file
std::vector<std::string> LoadWordList() {
    std::vector<std::string> words;
    std::ifstream file("word.csv");
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string word;
        while (std::getline(ss, word, ',')) {
            word.erase(std::remove(word.begin(), word.end(), '"'), word.end());
            word.erase(0, word.find_first_not_of(" \t"));
            word.erase(word.find_last_not_of(" \t") + 1);
            if (!word.empty()) {
                words.push_back(word);
            }
        }
    }
    if (words.size() != 2048) {
        throw std::runtime_error("Wordlist must contain exactly 2048 words.");
    }
    return words;
}

const std::vector<std::string>& GetWordList() {
    static const std::vector<std::string> wordlist = LoadWordList();
    return wordlist;
}

// Convert bytes to bit vector
std::vector<bool> BytesToBits(const std::vector<unsigned char>& bytes) {
    std::vector<bool> bits;
    for (unsigned char byte : bytes) {
        for (int i = 7; i >= 0; --i) {
            bits.push_back((byte >> i) & 1);
        }
    }
    return bits;
}

// Convert bit vector to integer
int BitsToInt(const std::vector<bool>& bits, size_t start, size_t length) {
    int value = 0;
    for (size_t i = 0; i < length; ++i) {
        value <<= 1;
        if (start + i < bits.size() && bits[start + i]) value |= 1;
    }
    return value;
}

std::string GenerateMnemonic(int entropy_size) {
    if (entropy_size % 32 != 0 || entropy_size < 128 || entropy_size > 256) {
        throw std::invalid_argument("Entropy size must be a multiple of 32, between 128 and 256 bits.");
    }

    std::vector<unsigned char> entropy(entropy_size / 8);
    GetStrongRandBytes(std::span<unsigned char>(entropy));

    // Calculate checksum
    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256().Write(entropy.data(), entropy.size()).Finalize(hash);
    int checksum_bits = entropy_size / 32;

    // Combine entropy and checksum bits
    std::vector<bool> bits = BytesToBits(entropy);
    for (int i = 0; i < checksum_bits; ++i) {
        bits.push_back((hash[0] >> (7 - i)) & 1);
    }

    // Convert to words
    const auto& wordlist = GetWordList();
    std::stringstream mnemonic;
    for (size_t i = 0; i < bits.size(); i += 11) {
        int idx = BitsToInt(bits, i, 11);
        mnemonic << wordlist[idx] << " ";
    }

    std::string result = mnemonic.str();
    result.pop_back(); // Remove trailing space
    return result;
}

bool ValidateMnemonic(const std::string& mnemonic) {
    const auto& wordlist = GetWordList();
    std::istringstream iss(mnemonic);
    std::vector<std::string> words;
    std::string word;
    while (iss >> word) {
        if (std::find(wordlist.begin(), wordlist.end(), word) == wordlist.end()) {
            return false;
        }
        words.push_back(word);
    }

    if (words.size() != 12 && words.size() != 15 && words.size() != 18 &&
        words.size() != 21 && words.size() != 24) {
        return false;
    }

    return true;
}

std::vector<unsigned char> MnemonicToSeed(const std::string& mnemonic, const std::string& passphrase) {
    if (!ValidateMnemonic(mnemonic)) {
        throw std::invalid_argument("Invalid mnemonic phrase.");
    }

    std::string salt = "mnemonic" + passphrase;
    std::vector<unsigned char> seed(64);

    PKCS5_PBKDF2_HMAC(
        mnemonic.c_str(), mnemonic.size(),
        reinterpret_cast<const unsigned char*>(salt.c_str()), salt.size(),
        2048,
        EVP_sha512(),
        64,
        seed.data()
    );

    return seed;
}

} // namespace wallet