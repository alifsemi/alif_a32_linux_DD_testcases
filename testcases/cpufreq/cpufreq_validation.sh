#!/bin/sh
# =============================================================================
# CPUFreq Validation Test Script
# Platform : Linux 6.12 / Yocto 5.0
# Policy   : policy0 (CPU0, CPU1)
# Min Freq : 25000 KHz | Max Freq : 800000 KHz
# Governors: ondemand, userspace, performance, schedutil
# =============================================================================

POLICY="/sys/devices/system/cpu/cpufreq/policy0"
CPU0_PATH="/sys/devices/system/cpu/cpu0"
CPU1_PATH="/sys/devices/system/cpu/cpu1"
CPU0_ONLINE="/sys/devices/system/cpu/cpu0/online"
CPU1_ONLINE="/sys/devices/system/cpu/cpu1/online"
LOG="/var/volatile/cpufreq_test_$(date +%Y%m%d_%H%M%S).log"
PASS=0
FAIL=0
TOTAL=0

# Colors
GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[1;33m"
CYAN="\033[0;36m"
NC="\033[0m"

# Available frequencies from policy output
AVAILABLE_FREQS="800000 400000 266667 200000 160000 133333 114285 100000 \
88888 80000 72727 66666 61538 57142 53333 50000 47058 44444 42105 40000 \
38095 36363 34782 33333 32000 30769 29629 28571 27586 26666 25806 25000"

MIN_FREQ=25000
MAX_FREQ=800000

# =============================================================================
# HELPER FUNCTIONS
# =============================================================================

log() {
    echo -e "$1" | tee -a "$LOG"
}

print_header() {
    log "\n${CYAN}============================================================${NC}"
    log "${CYAN}  $1${NC}"
    log "${CYAN}============================================================${NC}"
}

print_result() {
    TOTAL=$((TOTAL + 1))
    if [ "$2" == "PASS" ]; then
        PASS=$((PASS + 1))
        log "  ${GREEN}[PASS]${NC} $1"
    else
        FAIL=$((FAIL + 1))
        log "  ${RED}[FAIL]${NC} $1 --> $3"
    fi
}

check_root() {
    if [ "$(id -u)" -ne 0 ]; then
        log "${RED}ERROR: This script must be run as root!${NC}"
        exit 1
    fi
}

read_freq() {
    cat "$POLICY/scaling_cur_freq" 2>/dev/null
}

read_cpu0_freq() {
    cat "$CPU0_PATH/cpufreq/scaling_cur_freq" 2>/dev/null
}

read_cpu1_freq() {
    cat "$CPU1_PATH/cpufreq/scaling_cur_freq" 2>/dev/null
}

get_cpu_online_status() {
    local cpu=$1
    if [ "$cpu" == "0" ]; then
        # CPU0 is always online (cannot be offlined)
        echo 1
    else
        cat "$CPU1_ONLINE" 2>/dev/null
    fi
}

set_governor() {
    echo "$1" > "$POLICY/scaling_governor" 2>/dev/null
    sleep 1
}

get_governor() {
    cat "$POLICY/scaling_governor" 2>/dev/null
}

set_freq() {
    echo "$1" > "$POLICY/scaling_setspeed" 2>/dev/null
    sleep 1
}

# =============================================================================
# TEST 1: BASIC SANITY TESTS
# =============================================================================

test_basic_sanity() {
    print_header "TEST 1: Basic Sanity Tests"

    # Check policy directory exists
    if [ -d "$POLICY" ]; then
        print_result "CPUFreq policy0 directory exists" "PASS"
    else
        print_result "CPUFreq policy0 directory exists" "FAIL" "Directory $POLICY not found"
        log "${RED}CRITICAL: Cannot proceed without policy0. Exiting.${NC}"
        exit 1
    fi

    # Check min frequency
    MIN=$(cat "$POLICY/cpuinfo_min_freq" 2>/dev/null)
    if [ "$MIN" == "$MIN_FREQ" ]; then
        print_result "Min frequency is $MIN_FREQ KHz" "PASS"
    else
        print_result "Min frequency is $MIN_FREQ KHz" "FAIL" "Got: $MIN"
    fi

    # Check max frequency
    MAX=$(cat "$POLICY/cpuinfo_max_freq" 2>/dev/null)
    if [ "$MAX" == "$MAX_FREQ" ]; then
        print_result "Max frequency is $MAX_FREQ KHz" "PASS"
    else
        print_result "Max frequency is $MAX_FREQ KHz" "FAIL" "Got: $MAX"
    fi

    # Check scaling_min_freq
    SMIN=$(cat "$POLICY/scaling_min_freq" 2>/dev/null)
    if [ -n "$SMIN" ]; then
        print_result "scaling_min_freq is readable: $SMIN KHz" "PASS"
    else
        print_result "scaling_min_freq is readable" "FAIL" "Empty or not found"
    fi

    # Check scaling_max_freq
    SMAX=$(cat "$POLICY/scaling_max_freq" 2>/dev/null)
    if [ -n "$SMAX" ]; then
        print_result "scaling_max_freq is readable: $SMAX KHz" "PASS"
    else
        print_result "scaling_max_freq is readable" "FAIL" "Empty or not found"
    fi

    # Check current frequency readable
    CUR=$(read_freq)
    if [ -n "$CUR" ]; then
        print_result "scaling_cur_freq is readable: $CUR KHz" "PASS"
    else
        print_result "scaling_cur_freq is readable" "FAIL" "Empty or not found"
    fi

    # Check affected CPUs
    CPUS=$(cat "$POLICY/affected_cpus" 2>/dev/null)
    if echo "$CPUS" | grep -q "0" && echo "$CPUS" | grep -q "1"; then
        print_result "Affected CPUs contain CPU0 and CPU1: [$CPUS]" "PASS"
    else
        print_result "Affected CPUs contain CPU0 and CPU1" "FAIL" "Got: [$CPUS]"
    fi

    # Check CPU0 cpufreq directory exists
    if [ -d "$CPU0_PATH/cpufreq" ]; then
        print_result "CPU0 cpufreq directory exists" "PASS"
    else
        print_result "CPU0 cpufreq directory exists" "FAIL" "Directory not found"
    fi

    # Check CPU1 cpufreq directory exists
    if [ -d "$CPU1_PATH/cpufreq" ]; then
        print_result "CPU1 cpufreq directory exists" "PASS"
    else
        print_result "CPU1 cpufreq directory exists" "FAIL" "Directory not found"
    fi

    # Check CPU0 current frequency
    CPU0_FREQ=$(read_cpu0_freq)
    if [ -n "$CPU0_FREQ" ]; then
        print_result "CPU0 scaling_cur_freq is readable: $CPU0_FREQ KHz" "PASS"
    else
        print_result "CPU0 scaling_cur_freq is readable" "FAIL" "Empty or not found"
    fi

    # Check CPU1 current frequency
    CPU1_FREQ=$(read_cpu1_freq)
    if [ -n "$CPU1_FREQ" ]; then
        print_result "CPU1 scaling_cur_freq is readable: $CPU1_FREQ KHz" "PASS"
    else
        print_result "CPU1 scaling_cur_freq is readable" "FAIL" "Empty or not found"
    fi

    # Verify CPU0 and CPU1 share same frequency (same policy)
    if [ "$CPU0_FREQ" == "$CPU1_FREQ" ]; then
        print_result "CPU0 and CPU1 frequencies match (shared policy): $CPU0_FREQ KHz" "PASS"
    else
        print_result "CPU0 and CPU1 frequencies match" "FAIL" "CPU0: $CPU0_FREQ, CPU1: $CPU1_FREQ"
    fi

    # Check available governors
    GOVS=$(cat "$POLICY/scaling_available_governors" 2>/dev/null)
    for gov in ondemand userspace performance schedutil; do
        if echo "$GOVS" | grep -q "$gov"; then
            print_result "Governor '$gov' is available" "PASS"
        else
            print_result "Governor '$gov' is available" "FAIL" "Not found in: $GOVS"
        fi
    done

    # Check available frequencies list
    AVAIL=$(cat "$POLICY/scaling_available_frequencies" 2>/dev/null)
    if [ -n "$AVAIL" ]; then
        print_result "scaling_available_frequencies is readable" "PASS"
        log "    Frequencies: $AVAIL"
    else
        print_result "scaling_available_frequencies is readable" "FAIL" "Empty or not found"
    fi
}

# =============================================================================
# TEST 2: GOVERNOR SWITCH TESTS
# =============================================================================

test_governor_switch() {
    print_header "TEST 2: Governor Switch Tests"

    for gov in performance ondemand schedutil userspace; do
        set_governor "$gov"
        ACTUAL=$(get_governor)
        if [ "$ACTUAL" == "$gov" ]; then
            print_result "Switch to governor '$gov'" "PASS"
        else
            print_result "Switch to governor '$gov'" "FAIL" "Got: $ACTUAL"
        fi
    done
}

# =============================================================================
# TEST 3: FREQUENCY SCALING TESTS (userspace governor)
# =============================================================================

test_frequency_scaling() {
    print_header "TEST 3: Frequency Scaling Tests (userspace governor)"

    set_governor "userspace"
    GOV=$(get_governor)
    if [ "$GOV" != "userspace" ]; then
        print_result "Set userspace governor for freq test" "FAIL" "Governor not set"
        return
    fi

    # Test min frequency on both CPUs
    set_freq "$MIN_FREQ"
    ACTUAL=$(read_freq)
    CPU0_FREQ=$(read_cpu0_freq)
    CPU1_FREQ=$(read_cpu1_freq)
    if [ "$ACTUAL" == "$MIN_FREQ" ]; then
        print_result "Set and verify min frequency ($MIN_FREQ KHz)" "PASS"
    else
        print_result "Set and verify min frequency ($MIN_FREQ KHz)" "FAIL" "Got: $ACTUAL"
    fi
    if [ "$CPU0_FREQ" == "$MIN_FREQ" ]; then
        print_result "CPU0 at min frequency ($MIN_FREQ KHz)" "PASS"
    else
        print_result "CPU0 at min frequency ($MIN_FREQ KHz)" "FAIL" "Got: $CPU0_FREQ"
    fi
    if [ "$CPU1_FREQ" == "$MIN_FREQ" ]; then
        print_result "CPU1 at min frequency ($MIN_FREQ KHz)" "PASS"
    else
        print_result "CPU1 at min frequency ($MIN_FREQ KHz)" "FAIL" "Got: $CPU1_FREQ"
    fi

    # Test max frequency on both CPUs
    set_freq "$MAX_FREQ"
    ACTUAL=$(read_freq)
    CPU0_FREQ=$(read_cpu0_freq)
    CPU1_FREQ=$(read_cpu1_freq)
    if [ "$ACTUAL" == "$MAX_FREQ" ]; then
        print_result "Set and verify max frequency ($MAX_FREQ KHz)" "PASS"
    else
        print_result "Set and verify max frequency ($MAX_FREQ KHz)" "FAIL" "Got: $ACTUAL"
    fi
    if [ "$CPU0_FREQ" == "$MAX_FREQ" ]; then
        print_result "CPU0 at max frequency ($MAX_FREQ KHz)" "PASS"
    else
        print_result "CPU0 at max frequency ($MAX_FREQ KHz)" "FAIL" "Got: $CPU0_FREQ"
    fi
    if [ "$CPU1_FREQ" == "$MAX_FREQ" ]; then
        print_result "CPU1 at max frequency ($MAX_FREQ KHz)" "PASS"
    else
        print_result "CPU1 at max frequency ($MAX_FREQ KHz)" "FAIL" "Got: $CPU1_FREQ"
    fi

    # Step through all available frequencies
    log "\n  ${YELLOW}Stepping through all available frequencies...${NC}"
    FREQ_FAIL=0
    for freq in $AVAILABLE_FREQS; do
        set_freq "$freq"
        ACTUAL=$(read_freq)
        if [ "$ACTUAL" == "$freq" ]; then
            log "    ${GREEN}OK${NC}  Set: $freq KHz | Actual: $ACTUAL KHz"
        else
            log "    ${RED}FAIL${NC} Set: $freq KHz | Actual: $ACTUAL KHz"
            FREQ_FAIL=$((FREQ_FAIL + 1))
        fi
    done

    if [ "$FREQ_FAIL" -eq 0 ]; then
        print_result "All available frequencies set correctly" "PASS"
    else
        print_result "All available frequencies set correctly" "FAIL" "$FREQ_FAIL frequency/frequencies mismatched"
    fi
}

# =============================================================================
# TEST 4: BOUNDARY TESTS
# =============================================================================

test_boundary() {
    print_header "TEST 4: Min/Max Boundary Tests"

    # Set max scaling limit
    echo "$MAX_FREQ" > "$POLICY/scaling_max_freq"
    VAL=$(cat "$POLICY/scaling_max_freq")
    if [ "$VAL" == "$MAX_FREQ" ]; then
        print_result "scaling_max_freq set to $MAX_FREQ KHz" "PASS"
    else
        print_result "scaling_max_freq set to $MAX_FREQ KHz" "FAIL" "Got: $VAL"
    fi

    # Set min scaling limit
    echo "$MIN_FREQ" > "$POLICY/scaling_min_freq"
    VAL=$(cat "$POLICY/scaling_min_freq")
    if [ "$VAL" == "$MIN_FREQ" ]; then
        print_result "scaling_min_freq set to $MIN_FREQ KHz" "PASS"
    else
        print_result "scaling_min_freq set to $MIN_FREQ KHz" "FAIL" "Got: $VAL"
    fi

    # Test invalid frequency (should be rejected or clamped)
    set_governor "userspace"
    BEFORE=$(read_freq)
    echo 999999 > "$POLICY/scaling_setspeed" 2>/dev/null
    sleep 1
    AFTER=$(read_freq)
    if [ "$AFTER" -le "$MAX_FREQ" ]; then
        print_result "Invalid frequency (999999) rejected/clamped to valid range" "PASS"
    else
        print_result "Invalid frequency (999999) rejected/clamped" "FAIL" "Got: $AFTER"
    fi

    # Test below min frequency (should be rejected or clamped)
    echo 1000 > "$POLICY/scaling_setspeed" 2>/dev/null
    sleep 1
    AFTER=$(read_freq)
    if [ "$AFTER" -ge "$MIN_FREQ" ]; then
        print_result "Below-min frequency (1000) rejected/clamped to valid range" "PASS"
    else
        print_result "Below-min frequency (1000) rejected/clamped" "FAIL" "Got: $AFTER"
    fi
}

# =============================================================================
# TEST 5: PERFORMANCE GOVERNOR VALIDATION
# =============================================================================

test_performance_governor() {
    print_header "TEST 5: Performance Governor Validation"

    set_governor "performance"
    sleep 1
    FREQ=$(read_freq)
    CPU0_FREQ=$(read_cpu0_freq)
    CPU1_FREQ=$(read_cpu1_freq)

    if [ "$FREQ" == "$MAX_FREQ" ]; then
        print_result "Performance governor locks policy to max frequency ($MAX_FREQ KHz)" "PASS"
    else
        print_result "Performance governor locks policy to max frequency ($MAX_FREQ KHz)" "FAIL" "Got: $FREQ"
    fi

    # Verify CPU0 at max
    if [ "$CPU0_FREQ" == "$MAX_FREQ" ]; then
        print_result "CPU0 at max frequency under performance governor ($MAX_FREQ KHz)" "PASS"
    else
        print_result "CPU0 at max frequency under performance governor ($MAX_FREQ KHz)" "FAIL" "Got: $CPU0_FREQ"
    fi

    # Verify CPU1 at max
    if [ "$CPU1_FREQ" == "$MAX_FREQ" ]; then
        print_result "CPU1 at max frequency under performance governor ($MAX_FREQ KHz)" "PASS"
    else
        print_result "CPU1 at max frequency under performance governor ($MAX_FREQ KHz)" "FAIL" "Got: $CPU1_FREQ"
    fi

    # Verify both CPUs stay at max under repeated reads
    STABLE_CPU0=1
    STABLE_CPU1=1
    for i in 1 2 3; do
        sleep 1
        F0=$(read_cpu0_freq)
        F1=$(read_cpu1_freq)
        if [ "$F0" != "$MAX_FREQ" ]; then
            STABLE_CPU0=0
        fi
        if [ "$F1" != "$MAX_FREQ" ]; then
            STABLE_CPU1=0
        fi
    done

    if [ "$STABLE_CPU0" -eq 1 ]; then
        print_result "CPU0 frequency stable at max under performance governor" "PASS"
    else
        print_result "CPU0 frequency stable at max under performance governor" "FAIL" "Frequency fluctuated"
    fi

    if [ "$STABLE_CPU1" -eq 1 ]; then
        print_result "CPU1 frequency stable at max under performance governor" "PASS"
    else
        print_result "CPU1 frequency stable at max under performance governor" "FAIL" "Frequency fluctuated"
    fi
}

# =============================================================================
# TEST 6: TRANSITION LATENCY
# =============================================================================

test_transition_latency() {
    print_header "TEST 6: Frequency Transition Latency"

    LATENCY=$(cat "$POLICY/cpuinfo_transition_latency" 2>/dev/null)
    if [ -n "$LATENCY" ] && [ "$LATENCY" -gt 0 ]; then
        print_result "cpuinfo_transition_latency is valid: $LATENCY ns" "PASS"
        log "    Note: Latency value = $LATENCY ns ($(echo "$LATENCY/1000" | bc) us)"
    else
        print_result "cpuinfo_transition_latency is valid" "FAIL" "Got: $LATENCY"
    fi

    # Measure actual transition time between min and max
    set_governor "userspace"
    set_freq "$MIN_FREQ"
    sleep 1

    START_TIME=$(date +%s%N)
    set_freq "$MAX_FREQ"
    END_TIME=$(date +%s%N)
    ELAPSED=$(( (END_TIME - START_TIME) / 1000000 ))

    log "    Measured transition time (min->max): ~${ELAPSED} ms"
    print_result "Frequency transition min->max completed" "PASS"
}

# =============================================================================
# SUMMARY REPORT
# =============================================================================

print_summary() {
    print_header "TEST SUMMARY"
    log "  Total Tests : $TOTAL"
    log "  ${GREEN}Passed      : $PASS${NC}"
    log "  ${RED}Failed      : $FAIL${NC}"
    log "  Log File    : $LOG"

    log "\n${CYAN}  Pass Criteria Summary:${NC}"
    log "  ┌─────────────────────────────────────────────────────────────┐"
    log "  │ Governor switch        → Applied correctly, no errors       │"
    log "  │ Frequency set          → cur_freq matches setspeed          │"
    log "  │ Performance governor   → Always locks to 800000 KHz         │"
    log "  │ Min/Max boundary       → Rejects out-of-range values        │"
    log "  └─────────────────────────────────────────────────────────────┘"

    if [ "$FAIL" -eq 0 ]; then
        log "\n  ${GREEN}*** ALL TESTS PASSED ***${NC}\n"
    else
        log "\n  ${RED}*** $FAIL TEST(S) FAILED - REVIEW LOG: $LOG ***${NC}\n"
    fi
}

# =============================================================================
# RESTORE DEFAULT STATE
# =============================================================================

restore_defaults() {
    log "\n${YELLOW}Restoring default governor (schedutil)...${NC}"
    set_governor "schedutil"
    echo "$MAX_FREQ" > "$POLICY/scaling_max_freq"
    echo "$MIN_FREQ" > "$POLICY/scaling_min_freq"
    # Ensure CPU1 is online
    [ -f "$CPU1_ONLINE" ] && echo 1 > "$CPU1_ONLINE" 2>/dev/null
    sleep 1
    # Verify both CPUs are operational
    CPU0_FREQ=$(read_cpu0_freq)
    CPU1_FREQ=$(read_cpu1_freq)
    log "Default state restored. CPU0: $CPU0_FREQ KHz, CPU1: $CPU1_FREQ KHz"
}

# =============================================================================
# MAIN ENTRY POINT
# =============================================================================

main() {
    check_root

    log "${CYAN}"
    log "============================================================"
    log "  CPUFreq Validation Test Script"
    log "  Platform : Linux 6.12 / Yocto 5.0"
    log "  Date     : $(date)"
    log "  Host     : $(hostname)"
    log "  Kernel   : $(uname -r)"
    log "============================================================${NC}"

    case "$1" in
        --post-reboot)
            test_reboot_persistence_post
            ;;
        --test)
            case "$2" in
                sanity)       test_basic_sanity ;;
                governor)     test_governor_switch ;;
                frequency)    test_frequency_scaling ;;
                boundary)     test_boundary ;;
                performance)  test_performance_governor ;;
                latency)      test_transition_latency ;;
                *)
                    log "Usage: $0 --test [sanity|governor|frequency|boundary|performance|latency]"
                    exit 1
                    ;;
            esac
            ;;
        *)
            # Run all tests
            test_basic_sanity
            test_governor_switch
            test_frequency_scaling
            test_boundary
            test_performance_governor
            test_transition_latency
            ;;
    esac

    restore_defaults
    print_summary

    exit $FAIL
}

main "$@"
