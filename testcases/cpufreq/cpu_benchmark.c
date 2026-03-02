/*
 * CPU-bound benchmark for CPUFreq validation
 * Compile: arm-poky-linux-musl-gcc -O0 -o cpu_benchmark cpu_benchmark.c
 * Usage: ./cpu_benchmark [iterations]
 *
 * This performs pure integer arithmetic in a tight loop.
 * Execution time should scale linearly with CPU frequency.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_ITERATIONS 10000000

static volatile unsigned int result;

int main(int argc, char *argv[])
{
	unsigned long iterations = DEFAULT_ITERATIONS;
	unsigned long i;
	unsigned int a = 1, b = 2, c = 3;
	struct timespec start, end;
	long elapsed_ms;

	if (argc > 1)
		iterations = strtoul(argv[1], NULL, 10);

	printf("Running %lu iterations of CPU-bound arithmetic...\n", iterations);

	clock_gettime(CLOCK_MONOTONIC, &start);

	for (i = 0; i < iterations; i++) {
		a = a * 3 + b;
		b = b * 5 + c;
		c = c * 7 + a;
		a ^= (b << 3);
		b ^= (c >> 2);
		c ^= (a << 1);
	}

	clock_gettime(CLOCK_MONOTONIC, &end);

	/* Store result to prevent optimization */
	result = a + b + c;

	elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 +
		     (end.tv_nsec - start.tv_nsec) / 1000000;

	printf("Elapsed: %ld ms\n", elapsed_ms);
	printf("Result: %u (to prevent optimization)\n", result);

	return 0;
}
