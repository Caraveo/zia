#include <wallet/mnemonic.h>
#include <random.h>
#include <util/strencodings.h>
#include <crypto/sha256.h>
#include <crypto/sha512.h>
#include <crypto/hmac_sha512.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>
#include <span>

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
            // Remove quotes and trim whitespace
            word.erase(std::remove(word.begin(), word.end(), '"'), word.end());
            word.erase(0, word.find_first_not_of(" \t"));
            word.erase(word.find_last_not_of(" \t") + 1);
            if (!word.empty()) {
                words.push_back(word);
            }
        }
    }
    return words;
}

// Get wordlist (lazy loaded)
const std::vector<std::string>& GetWordList() {
    static const std::vector<std::string> wordlist = LoadWordList();
    return wordlist;
}

std::string GenerateMnemonic(int entropy_size) {
    if (entropy_size % 32 != 0 || entropy_size < 128 || entropy_size > 256) {
        throw std::invalid_argument("Entropy size must be a multiple of 32, between 128 and 256 bits.");
    }

    std::vector<unsigned char> entropy(entropy_size / 8);
    std::span<unsigned char> entropy_span(entropy.data(), entropy.size());
    GetStrongRandBytes(entropy_span);

    // Calculate checksum
    unsigned char hash[CSHA256::OUTPUT_SIZE];
    CSHA256 sha256;
    sha256.Write(entropy.data(), entropy.size()).Finalize(hash);
    int checksum_bits = entropy_size / 32;
    unsigned char checksum = hash[0] >> (8 - checksum_bits);

    // Combine entropy and checksum
    std::vector<unsigned char> combined(entropy);
    combined.push_back(checksum);

    // Convert to mnemonic
    const auto& wordlist = GetWordList();
    std::stringstream mnemonic;
    for (size_t i = 0; i < combined.size() * 8 / 11; ++i) {
        int index = 0;
        for (int j = 0; j < 11; ++j) {
            int bit = (combined[i * 11 + j / 8] >> (7 - (j % 8))) & 1;
            index = (index << 1) | bit;
        }
        mnemonic << wordlist[index] << " ";
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

    if (words.size() != 12 && words.size() != 15 && words.size() != 18 && words.size() != 21 && words.size() != 24) {
        return false;
    }

    return true;
}

std::vector<unsigned char> MnemonicToSeed(const std::string& mnemonic, const std::string& passphrase) {
    if (!ValidateMnemonic(mnemonic)) {
        throw std::invalid_argument("Invalid mnemonic phrase.");
    }

    std::vector<unsigned char> seed(64);
    CHMAC_SHA512 hmac((unsigned char*)"Bitcoin seed", 12);
    hmac.Write((unsigned char*)mnemonic.c_str(), mnemonic.size())
        .Write((unsigned char*)passphrase.c_str(), passphrase.size())
        .Finalize(seed.data());
    return seed;
}

} // namespace wallet 