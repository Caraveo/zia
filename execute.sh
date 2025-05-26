#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
ORANGE='\033[38;5;208m'
BOLD='\033[1m'
BLINK='\033[5m'
NC='\033[0m' # No Color

# Error handling
set -eE  # Exit on error
trap 'handle_error $? $LINENO' ERR

# Global variables
LOG_FILE="zia_cleanup_$(date +%Y%m%d_%H%M%S).log"
TEMP_FILES=()
CLEANUP_PIDS=()
MAX_RETRIES=3
TIMEOUT_SECONDS=300  # 5 minutes timeout for long operations

# Function to handle errors
handle_error() {
    local exit_code=$1
    local line_number=$2
    local error_msg="Error occurred in script at line $line_number with exit code $exit_code"
    
    log "ERROR" "$error_msg"
    log "ERROR" "Stack trace:"
    local frame=0
    while caller $frame > /dev/null; do
        local trace=$(caller $frame)
        log "ERROR" "  $trace"
        ((frame++))
    done
    
    cleanup
    exit $exit_code
}

# Function to handle timeouts
handle_timeout() {
    local pid=$1
    local operation=$2
    
    log "ERROR" "Operation '$operation' timed out after ${TIMEOUT_SECONDS} seconds"
    kill -9 $pid 2>/dev/null || true
    cleanup
    exit 1
}

# Function to cleanup temporary files and processes
cleanup() {
    log "INFO" "Performing cleanup..."
    
    # Kill any remaining background processes
    for pid in "${CLEANUP_PIDS[@]}"; do
        if ps -p $pid > /dev/null; then
            log "INFO" "Killing process $pid"
            kill -9 $pid 2>/dev/null || true
        fi
    done
    
    # Remove temporary files
    for file in "${TEMP_FILES[@]}"; do
        if [ -f "$file" ]; then
            log "INFO" "Removing temporary file: $file"
            rm -f "$file"
        fi
    done
    
    # Kill daemon if it's running
    if pgrep -x "ziacoind" > /dev/null; then
        log "INFO" "Stopping ZiaCoin daemon"
        ./build/bin/ziacoin-cli stop > /dev/null 2>&1 || true
        sleep 2
        pkill -x "ziacoind" 2>/dev/null || true
    fi
}

# Function to log messages with colors
log() {
    local level=$1
    local message=$2
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local color=$NC
    
    case $level in
        "ERROR") color=$RED ;;
        "SUCCESS") color=$GREEN ;;
        "WARNING") color=$YELLOW ;;
        "INFO") color=$BLUE ;;
    esac
    
    echo -e "${timestamp} ${color}[${level}]${NC} ${message}" | tee -a "$LOG_FILE"
}

# Function to check if a command succeeded with retries
check_status_with_retry() {
    local max_attempts=$1
    local delay=$2
    local success_msg=$3
    local error_msg=$4
    shift 4
    
    local attempt=1
    while [ $attempt -le $max_attempts ]; do
        if "$@"; then
            log "SUCCESS" "$success_msg"
            return 0
        fi
        
        if [ $attempt -lt $max_attempts ]; then
            log "WARNING" "Attempt $attempt failed. Retrying in $delay seconds..."
            sleep $delay
        fi
        ((attempt++))
    done
    
    log "ERROR" "$error_msg (after $max_attempts attempts)"
    return 1
}

# Function to monitor process with timeout
monitor_process() {
    local pid=$1
    local timeout=$2
    local operation=$3
    
    (
        sleep $timeout
        if ps -p $pid > /dev/null; then
            handle_timeout $pid "$operation"
        fi
    ) &
    CLEANUP_PIDS+=($!)
}

# Function to clean genesis files
clean_genesis_files() {
    log "INFO" "Cleaning genesis-related files..."
    
    # List of genesis files to remove
    GENESIS_FILES=(
        "zia_genesis_output.txt"
        "mining_params.json"
        "src/zia_genesis_output.txt"
    )
    
    for file in "${GENESIS_FILES[@]}"; do
        if [ -f "$file" ]; then
            log "INFO" "Removing file: $file"
            rm -f "$file"
            check_status_with_retry 3 2 "Removed $file" "Failed to remove $file" true
        else
            log "WARNING" "File does not exist: $file"
        fi
    done
    
    log "SUCCESS" "Genesis files cleanup completed"
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
        "debug.log"
        "db.log"
    )
    
    # Remove directories
    for dir in "${DIRS_TO_REMOVE[@]}"; do
        if [ -d "$DATA_DIR/$dir" ]; then
            log "INFO" "Removing directory: $dir"
            rm -rf "$DATA_DIR/$dir"
            check_status_with_retry 3 2 "Removed $dir" "Failed to remove $dir" true
        else
            log "WARNING" "Directory does not exist: $dir"
        fi
    done
    
    # Remove files
    for file in "${FILES_TO_REMOVE[@]}"; do
        if [ -f "$DATA_DIR/$file" ]; then
            log "INFO" "Removing file: $file"
            rm -f "$DATA_DIR/$file"
            check_status_with_retry 3 2 "Removed $file" "Failed to remove $file" true
        else
            log "WARNING" "File does not exist: $file"
        fi
    done
    
    # Create necessary directories
    log "INFO" "Creating blockchain directories..."
    mkdir -p "$DATA_DIR/blocks"
    mkdir -p "$DATA_DIR/chainstate"
    mkdir -p "$DATA_DIR/wallets"
    
    log "SUCCESS" "Blockchain cleanup completed"
}

# Function to show progress animation
show_progress() {
    local pid=$1
    local message=$2
    local stage=$3
    local delay=0.1
    local spinstr='|/-\'
    local log_file="/tmp/build_progress_$$.log"
    TEMP_FILES+=("$log_file")
    
    # Set color based on stage
    local color=$BLUE
    if [ "$stage" = "config" ]; then
        color="${BLINK}${YELLOW}"
    elif [ "$stage" = "build" ]; then
        color="${BLINK}${ORANGE}"
    fi
    
    # Start monitoring the process output
    if [ "$stage" = "build" ]; then
        # Create a temporary log file
        touch "$log_file"
        # Start monitoring the process output in background
        while true; do
            if [ -f "$log_file" ]; then
                # Look for percentage in various formats
                local percent=$(grep -E '[0-9]|[0-9]+ percent|[0-9]+% complete' "$log_file" | tail -n1 | sed -E 's/.*([0-9]+).*/\1/')
                if [ ! -z "$percent" ]; then
                    # Just keep monitoring but don't display the number
                    sleep 3
                fi
            fi
            sleep 3
        done &
        CLEANUP_PIDS+=($!)
    fi
    
    while ps -p $pid > /dev/null; do
        # Update spinner
        local temp=${spinstr#?}
        printf "\r${color}%s [%c]${NC}" "$message" "$spinstr"
        local spinstr=$temp${spinstr%"$temp"}
        
        # Sleep for animation
        sleep $delay
    done
    
    printf "\r${GREEN}%s [✓]${NC}\n" "$message"
}

# Function to regenerate genesis block
regenerate_genesis() {
    log "INFO" "Regenerating genesis block..."
    
    # Run genesis generation in background
    ./run.sh --regen-genesis > /tmp/genesis_output.log 2>&1 &
    local genesis_pid=$!
    CLEANUP_PIDS+=($genesis_pid)
    
    # Monitor the process
    local timeout=300  # 5 minutes timeout
    local start_time=$(date +%s)
    
    while true; do
        # Check if process is still running
        if ! ps -p $genesis_pid > /dev/null; then
            # Process finished, check exit status
            wait $genesis_pid
            local exit_status=$?
            if [ $exit_status -eq 0 ]; then
                log "SUCCESS" "Genesis block regenerated successfully"
                return 0
            else
                log "ERROR" "Genesis block regeneration failed with exit code $exit_status"
                return 1
            fi
        fi
        
        # Check for timeout
        local current_time=$(date +%s)
        local elapsed=$((current_time - start_time))
        if [ $elapsed -gt $timeout ]; then
            log "ERROR" "Genesis block regeneration timed out after ${timeout} seconds"
            kill -9 $genesis_pid 2>/dev/null || true
            return 1
        fi
        
        # Show progress
        echo -ne "\r${YELLOW}Configuring Genesis Block [${spinstr:i++%${#spinstr}:1}]${NC}"
        sleep 0.1
    done
}

# Function to rebuild project
rebuild_project() {
    log "INFO" "Rebuilding project..."
    
    # Remove build directory
    if [ -d "build" ]; then
        log "INFO" "Removing build directory"
        rm -rf build
        check_status_with_retry 3 2 "Removed build directory" "Failed to remove build directory" true
    fi
    
    # Create a temporary log file for progress monitoring
    local build_log="/tmp/build_progress_$$.log"
    TEMP_FILES+=("$build_log")
    touch "$build_log"
    
    # Run build script with output redirected to log file
    log "INFO" "Running build script"
    ./run.sh > "$build_log" 2>&1 &
    local build_pid=$!
    
    # Monitor process with timeout
    monitor_process $build_pid $TIMEOUT_SECONDS "Project build"
    
    # Show progress animation for configuration
    show_progress $build_pid "Configuring Build" "config" &
    local config_pid=$!
    CLEANUP_PIDS+=($config_pid)
    
    # Wait for configuration to complete (first 30%)
    sleep 5
    
    # Kill config animation
    kill $config_pid 2>/dev/null || true
    
    # Show progress animation for build
    show_progress $build_pid "Building ZiaCoin" "build" &
    local progress_pid=$!
    CLEANUP_PIDS+=($progress_pid)
    
    # Wait for the actual process to complete
    wait $build_pid
    local build_status=$?
    
    # Kill progress animation
    kill $progress_pid 2>/dev/null || true
    
    check_status_with_retry 3 2 "Build completed successfully" "Build failed" [ $build_status -eq 0 ]
    
    # Verify build artifacts
    if [ -f "build/bin/ziacoind" ] && [ -f "build/bin/ziacoin-cli" ]; then
        log "SUCCESS" "Build artifacts created successfully"
        log "INFO" "Created: build/bin/ziacoind"
        log "INFO" "Created: build/bin/ziacoin-cli"
    else
        log "ERROR" "Build artifacts not found"
        exit 1
    fi
}

# Function to start daemon
start_daemon() {
    log "INFO" "Starting ZiaCoin daemon..."
    
    # First attempt with reindex
    log "INFO" "Starting daemon with reindex..."
    ./build/bin/ziacoind --daemon --reindex
    check_status_with_retry 3 2 "Daemon started with reindex" "Failed to start daemon" true
    
    # Wait for daemon to start and RPC to be available
    log "INFO" "Waiting for daemon to start..."
    local daemon_ready=false
    for i in {1..30}; do
        if pgrep -x "ziacoind" > /dev/null; then
            # Try to get blockchain info to verify RPC is ready
            if ./build/bin/ziacoin-cli getblockchaininfo > /dev/null 2>&1; then
                log "SUCCESS" "Daemon is running and RPC is ready"
                daemon_ready=true
                break
            fi
        fi
        log "INFO" "Waiting for daemon to be ready... ($i/30)"
        sleep 1
    done
    
    if [ "$daemon_ready" = false ]; then
        # If first attempt fails, try with reindex-chainstate
        log "WARNING" "First attempt failed, trying with reindex-chainstate..."
        pkill -x "ziacoind" 2>/dev/null || true
        sleep 2
        
        ./build/bin/ziacoind --daemon --reindex-chainstate
        check_status_with_retry 3 2 "Daemon started with reindex-chainstate" "Failed to start daemon" true
        
        # Wait again
        for i in {1..30}; do
            if pgrep -x "ziacoind" > /dev/null; then
                if ./build/bin/ziacoin-cli getblockchaininfo > /dev/null 2>&1; then
                    log "SUCCESS" "Daemon is running and RPC is ready"
                    return 0
                fi
            fi
            log "INFO" "Waiting for daemon to be ready... ($i/30)"
            sleep 1
        done
        
        log "ERROR" "Daemon failed to start after multiple attempts"
        exit 1
    fi
}

# Function to check blockchain info
check_blockchain() {
    log "INFO" "Checking blockchain info..."
    local blockchain_info
    blockchain_info=$(./build/bin/ziacoin-cli getblockchaininfo)
    
    if [ $? -eq 0 ]; then
        log "SUCCESS" "Blockchain info retrieved"
        log "INFO" "Blockchain info:"
        echo "$blockchain_info" | while IFS= read -r line; do
            log "INFO" "  $line"
        done
    else
        log "ERROR" "Failed to get blockchain info"
        exit 1
    fi
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
    clean_genesis_files
    clean_blockchain
    regenerate_genesis
    rebuild_project
    start_daemon
    check_blockchain
    
    log "SUCCESS" "Process completed successfully"
    log "INFO" "Check $LOG_FILE for detailed logs"
}

# Set up cleanup trap
trap cleanup EXIT

# Run main function
main 