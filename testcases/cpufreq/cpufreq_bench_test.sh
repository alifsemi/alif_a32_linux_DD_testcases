#!/bin/sh
# CPUFreq Benchmark Test using compiled C program
# This test validates that CPU frequency changes affect execution time
# Tests all 32 frequencies supported by the Alif Ensemble CPUFreq driver
# Note: Uses only POSIX shell builtins, no awk required

CPUFREQ_PATH="/sys/devices/system/cpu/cpu0/cpufreq"
BENCHMARK="./cpu_benchmark"
ITERATIONS=5000000

# All supported frequencies (PLL 800MHz / divider 1-32)
FREQS="800000 400000 266667 200000 160000 133333 114285 100000 88888 80000 72727 66666 61538 57142 53333 50000 47058 44444 42105 40000 38095 36363 34782 33333 32000 30769 29629 28571 27586 26666 25806 25000"

echo "=============================================="
echo "  CPUFreq Benchmark Test - Alif Ensemble"
echo "=============================================="

# Check if benchmark exists
if [ ! -x "$BENCHMARK" ]; then
    echo "ERROR: $BENCHMARK not found or not executable"
    echo "Compile with: arm-none-linux-gnueabihf-gcc -O0 -o cpu_benchmark cpu_benchmark.c"
    exit 1
fi

# Set userspace governor
echo "userspace" > ${CPUFREQ_PATH}/scaling_governor 2>/dev/null

echo ""
echo "Testing CPU-bound benchmark at all frequencies..."
echo "Iterations: $ITERATIONS"
echo "PLL Base: 800 MHz, Dividers: 1-32"
echo ""
echo "Freq (kHz)   Div   Time (ms)   BogoMIPS    Ratio    Expected"
echo "----------   ---   ---------   --------    -----    --------"

# Get baseline at 800 MHz first
echo "800000" > ${CPUFREQ_PATH}/scaling_setspeed
sleep 1
baseline_bogomips=$(grep BogoMIPS /proc/cpuinfo | head -1 | sed 's/.*: *//')
baseline_output=$($BENCHMARK $ITERATIONS 2>&1)
baseline_ms=$(echo "$baseline_output" | grep "Elapsed:" | sed 's/[^0-9]//g')

echo "800000       1     ${baseline_ms}         ${baseline_bogomips}       1.00x    (baseline)"

# Test all other frequencies
div=2
for freq in 400000 266667 200000 160000 133333 114285 100000 88888 80000 72727 66666 61538 57142 53333 50000 47058 44444 42105 40000 38095 36363 34782 33333 32000 30769 29629 28571 27586 26666 25806 25000; do
    echo "$freq" > ${CPUFREQ_PATH}/scaling_setspeed
    sleep 1

    bogomips=$(grep BogoMIPS /proc/cpuinfo | head -1 | sed 's/.*: *//')
    output=$($BENCHMARK $ITERATIONS 2>&1)
    elapsed_ms=$(echo "$output" | grep "Elapsed:" | sed 's/[^0-9]//g')

    # Calculate actual ratio (integer math)
    if [ "$baseline_ms" -gt 0 ]; then
        ratio_x100=$((elapsed_ms * 100 / baseline_ms))
        ratio_int=$((ratio_x100 / 100))
        ratio_frac=$((ratio_x100 % 100))
        if [ $ratio_frac -lt 10 ]; then
            ratio="${ratio_int}.0${ratio_frac}x"
        else
            ratio="${ratio_int}.${ratio_frac}x"
        fi
    else
        ratio="N/A"
    fi

    echo "${freq}       ${div}     ${elapsed_ms}         ${bogomips}       ${ratio}    ${div}.00x"

    div=$((div + 1))
done

# Restore max frequency
echo "800000" > ${CPUFREQ_PATH}/scaling_setspeed

echo ""
echo ""
echo "Note: Small ratio drift (~2-3%) at lower frequencies is expected"
echo "      due to fixed measurement overhead (syscalls, loop setup)."
echo "      BogoMIPS values provide the most accurate frequency indication."
echo ""
echo "Test complete. Current freq: $(cat ${CPUFREQ_PATH}/scaling_cur_freq) kHz"
