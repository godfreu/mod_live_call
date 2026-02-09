#!/bin/bash

###############################################################################
# Test Script untuk mod_live_call
# 
# Usage:
#   ./scripts/test.sh [basic|stress|concurrent]
###############################################################################

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
FS_CLI=${FS_CLI:-fs_cli}
TEST_EXTENSION=${TEST_EXTENSION:-9000}
SIP_DOMAIN=${SIP_DOMAIN:-localhost}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
}

check_module_loaded() {
    log_test "Checking if mod_live_call is loaded..."
    
    if $FS_CLI -x "module_exists mod_live_call" | grep -q "true"; then
        log_pass "Module is loaded"
        return 0
    else
        log_fail "Module is not loaded"
        log_info "Try: fs_cli -x 'load mod_live_call'"
        return 1
    fi
}

check_api_available() {
    log_test "Checking API availability..."
    
    if $FS_CLI -x "live_call status" | grep -q "OK"; then
        log_pass "API is available"
        return 0
    else
        log_fail "API is not available"
        return 1
    fi
}

test_basic() {
    log_info "Running basic tests..."
    
    check_module_loaded || exit 1
    check_api_available || exit 1
    
    log_test "Getting module status..."
    $FS_CLI -x "live_call status"
    
    log_test "Getting module statistics..."
    $FS_CLI -x "live_call stats"
    
    log_pass "Basic tests completed"
}

test_single_call() {
    log_test "Testing single call..."
    
    # Originate a test call
    UUID=$($FS_CLI -x "originate {ignore_early_media=true}sofia/internal/1000@$SIP_DOMAIN $TEST_EXTENSION XML default" | grep -oP '[0-9a-f-]{36}' | head -1)
    
    if [ -z "$UUID" ]; then
        log_fail "Failed to originate call"
        return 1
    fi
    
    log_info "Call UUID: $UUID"
    
    # Wait for call to establish
    sleep 2
    
    # Check if streaming is active
    STATUS=$($FS_CLI -x "uuid_exists $UUID")
    if [ "$STATUS" == "true" ]; then
        log_pass "Call is active"
        
        # Get stats
        $FS_CLI -x "live_call stats"
        
        # Wait a bit
        sleep 5
        
        # Hangup
        $FS_CLI -x "uuid_kill $UUID"
        log_info "Call terminated"
    else
        log_fail "Call not found"
        return 1
    fi
    
    log_pass "Single call test completed"
}

test_concurrent() {
    local NUM_CALLS=${1:-10}
    log_test "Testing $NUM_CALLS concurrent calls..."
    
    UUIDS=()
    
    # Start calls
    log_info "Starting $NUM_CALLS calls..."
    for i in $(seq 1 $NUM_CALLS); do
        UUID=$($FS_CLI -x "bgapi originate {ignore_early_media=true}sofia/internal/1000@$SIP_DOMAIN $TEST_EXTENSION XML default" | grep -oP 'Job-UUID: \K[0-9a-f-]+')
        UUIDS+=($UUID)
        echo -n "."
    done
    echo ""
    
    # Wait for calls to establish
    log_info "Waiting for calls to establish..."
    sleep 5
    
    # Check stats
    log_info "Current statistics:"
    $FS_CLI -x "live_call stats"
    
    # Wait a bit
    log_info "Running for 10 seconds..."
    sleep 10
    
    # Hangup all calls
    log_info "Terminating calls..."
    $FS_CLI -x "hupall"
    
    sleep 2
    
    # Final stats
    log_info "Final statistics:"
    $FS_CLI -x "live_call stats"
    
    log_pass "Concurrent test completed"
}

test_stress() {
    local NUM_CALLS=${1:-100}
    local CALL_RATE=${2:-10}
    
    log_test "Stress testing with $NUM_CALLS calls at $CALL_RATE cps..."
    
    # Check if SIPp is available
    if ! command -v sipp &> /dev/null; then
        log_fail "SIPp not found. Please install SIPp for stress testing."
        log_info "Install: apt-get install sipp"
        return 1
    fi
    
    log_info "Starting SIPp..."
    sipp -sn uac \
         -d 10000 \
         -s $TEST_EXTENSION \
         -r $CALL_RATE \
         -l $NUM_CALLS \
         -m $NUM_CALLS \
         $SIP_DOMAIN &
    
    SIPP_PID=$!
    
    # Monitor stats
    log_info "Monitoring (press Ctrl+C to stop)..."
    while kill -0 $SIPP_PID 2>/dev/null; do
        clear
        echo "=== Live Statistics ==="
        $FS_CLI -x "live_call stats"
        echo ""
        echo "SIPp PID: $SIPP_PID"
        sleep 2
    done
    
    log_pass "Stress test completed"
}

test_memory_leak() {
    log_test "Testing for memory leaks..."
    
    # Get initial memory
    INITIAL_MEM=$(ps aux | grep freeswitch | grep -v grep | awk '{print $6}')
    log_info "Initial memory: ${INITIAL_MEM} KB"
    
    # Run multiple call cycles
    for cycle in {1..5}; do
        log_info "Cycle $cycle/5..."
        test_concurrent 20
        sleep 3
    done
    
    # Get final memory
    FINAL_MEM=$(ps aux | grep freeswitch | grep -v grep | awk '{print $6}')
    log_info "Final memory: ${FINAL_MEM} KB"
    
    # Calculate difference
    MEM_DIFF=$((FINAL_MEM - INITIAL_MEM))
    MEM_DIFF_MB=$((MEM_DIFF / 1024))
    
    log_info "Memory difference: ${MEM_DIFF_MB} MB"
    
    if [ $MEM_DIFF_MB -lt 100 ]; then
        log_pass "Memory usage is acceptable"
    else
        log_fail "Possible memory leak detected"
    fi
}

show_help() {
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  basic              - Run basic functionality tests"
    echo "  single             - Test single call"
    echo "  concurrent [N]     - Test N concurrent calls (default: 10)"
    echo "  stress [N] [rate]  - Stress test with N calls at rate cps (requires SIPp)"
    echo "  memory             - Memory leak test"
    echo "  help               - Show this help"
    echo ""
    echo "Environment variables:"
    echo "  FS_CLI          - Path to fs_cli (default: fs_cli)"
    echo "  TEST_EXTENSION  - Extension to dial (default: 9000)"
    echo "  SIP_DOMAIN      - SIP domain (default: localhost)"
    echo ""
    echo "Examples:"
    echo "  $0 basic"
    echo "  $0 concurrent 50"
    echo "  $0 stress 1000 50"
}

# Main
case "${1:-basic}" in
    basic)
        test_basic
        ;;
    single)
        test_single_call
        ;;
    concurrent)
        test_concurrent ${2:-10}
        ;;
    stress)
        test_stress ${2:-100} ${3:-10}
        ;;
    memory)
        test_memory_leak
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        log_fail "Unknown command: $1"
        show_help
        exit 1
        ;;
esac
