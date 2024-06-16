#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void test(FILE* input, FILE* output) {
	long x = random() % 1000;
	fprintf(input, "%ld\n", x);

	long sz = x * 3;

	long* xs = malloc(sizeof(long) * (size_t)sz);

	for (long i = 0; i < sz; i++) {
		xs[i] = i * 2;
		fprintf(output, "%ld\n", xs[i]);
	}

	free(xs);
}

int main(int argc, char* argv[]) {
	assert(argc >= 3);

	const char* input_name = argv[1];
	const char* output_name = argv[2];

	FILE* input = fopen(input_name, "w");
	FILE* output = fopen(output_name, "w");

	srandom(420);
	test(input, output);
	return 0;
}
