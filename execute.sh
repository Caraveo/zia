#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Log file
LOG_FILE="zia_cleanup_$(date +%Y%m%d_%H%M%S).log"

# Function to log messages
log() {
    local level=$1
    local message=$2
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo -e "${timestamp} [${level}] ${message}" | tee -a "$LOG_FILE"
}

# Function to check if a command succeeded
check_status() {
    if [ $? -eq 0 ]; then
        log "SUCCESS" "$1"
    else
        log "ERROR" "$2"
        exit 1
    fi
}

# Function to clean blockchain data
clean_blockchain() {
    log "INFO" "Starting blockchain cleanup..."
    
    # Define data directory
    DATA_DIR="$HOME/Library/Application Support/ZiaCoin"
    
    # List of directories and files to remove
    DIRS_TO_REMOVE=(
        "blocks"
        "chainstate"
        "wallets"
    )
    
    FILES_TO_REMOVE=(
        "peers.dat"
        "banlist.dat"
        "mempool.dat"
        "wallet.dat"
    )
    
    # Remove directories
    for dir in "${DIRS_TO_REMOVE[@]}"; do
        if [ -d "$DATA_DIR/$dir" ]; then
            log "INFO" "Removing directory: $dir"
            rm -rf "$DATA_DIR/$dir"
            check_status "Removed $dir" "Failed to remove $dir"
        else
            log "INFO" "Directory does not exist: $dir"
        fi
    done
    
    # Remove files
    for file in "${FILES_TO_REMOVE[@]}"; do
        if [ -f "$DATA_DIR/$file" ]; then
            log "INFO" "Removing file: $file"
            rm -f "$DATA_DIR/$file"
            check_status "Removed $file" "Failed to remove $file"
        else
            log "INFO" "File does not exist: $file"
        fi
    done
    
    log "INFO" "Blockchain cleanup completed"
}

# Function to regenerate genesis block
regenerate_genesis() {
    log "INFO" "Regenerating genesis block..."
    ./run.sh --regen-genesis
    check_status "Genesis block regenerated" "Failed to regenerate genesis block"
}

# Function to rebuild project
rebuild_project() {
    log "INFO" "Rebuilding project..."
    
    # Remove build directory
    if [ -d "build" ]; then
        log "INFO" "Removing build directory"
        rm -rf build
        check_status "Removed build directory" "Failed to remove build directory"
    fi
    
    # Run build script
    log "INFO" "Running build script"
    ./run.sh
    check_status "Project rebuilt" "Failed to rebuild project"
}

# Function to start daemon
start_daemon() {
    log "INFO" "Starting ZiaCoin daemon..."
    ./build/bin/ziacoind --daemon
    check_status "Daemon started" "Failed to start daemon"
    
    # Wait for daemon to start
    log "INFO" "Waiting for daemon to start..."
    sleep 5
    
    # Check if daemon is running
    if pgrep -x "ziacoind" > /dev/null; then
        log "INFO" "Daemon is running"
    else
        log "ERROR" "Daemon failed to start"
        exit 1
    fi
}

# Function to check blockchain info
check_blockchain() {
    log "INFO" "Checking blockchain info..."
    ./build/bin/ziacoin-cli getblockchaininfo
    check_status "Blockchain info retrieved" "Failed to get blockchain info"
}

# Main execution
main() {
    log "INFO" "Starting ZiaCoin blockchain cleanup and reset"
    log "INFO" "Log file: $LOG_FILE"
    
    # Check if we're in the correct directory
    if [ ! -f "run.sh" ]; then
        log "ERROR" "Must be run from the ZiaCoin root directory"
        exit 1
    fi
    
    # Execute steps
    clean_blockchain
    regenerate_genesis
    rebuild_project
    start_daemon
    check_blockchain
    
    log "INFO" "Process completed successfully"
    log "INFO" "Check $LOG_FILE for detailed logs"
}

# Run main function
main 