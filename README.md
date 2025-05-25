# ZiaCoin Network

ZiaCoin is a modern cryptocurrency implementation focused on security, privacy, and user-friendly features. Built on the foundations of ZiaCoin Core, ZiaCoin adds enhanced functionality while maintaining the robust security model of its predecessor.

## Features

- **Mnemonic Phrase Generation**: Generate secure mnemonic phrases with configurable entropy (128, 160, 192, 224, 256 bits)
- **HD Wallet Support**: Hierarchical Deterministic (HD) wallet implementation for better key management
- **Descriptor Wallets**: Modern wallet architecture using output script descriptors
- **External Signer Support**: Hardware wallet integration capabilities
- **SQLite Database**: Efficient and reliable wallet storage

## Building

### Prerequisites

- macOS (primary development platform)
- C++17 compatible compiler
- CMake 3.16 or higher
- SQLite3 development libraries

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/caraveo/zia.git
cd zia

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## Usage

### Command Line Interface

The ZiaCoin CLI provides various wallet management commands:

```bash
# Generate a new mnemonic phrase
./ziacoin-cli createphrase 256

# Create a new wallet
./ziacoin-cli createwallet "mywallet"

# Get wallet information
./ziacoin-cli getwalletinfo
```

### RPC API

ZiaCoin provides a comprehensive RPC API for programmatic interaction:

```json
// Generate mnemonic phrase
{
  "method": "createphrase",
  "params": [256]
}

// Create wallet
{
  "method": "createwallet",
  "params": ["mywallet", true, false, "passphrase", true, true]
}
```

## Development

### Project Structure

- `src/wallet/`: Wallet implementation
  - `rpc/`: RPC command handlers
  - `mnemonic.h`: Mnemonic phrase generation
  - `wallet.h`: Core wallet functionality

### Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Based on ZiaCoin Core
- Uses BIP32/39/44 for HD wallet implementation
- Implements BIP174 for Partially Signed ZiaCoin Transactions (PSBT)
