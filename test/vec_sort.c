#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void test(FILE* input, FILE* output) {
	long xs[100];

	for (size_t i = 0; i < 100; i++) {
		xs[i] = random();
		fprintf(input, "%ld\n", xs[i]);
	}

	for (size_t i = 1; i < 100; i++) {
		long key = xs[i];
		size_t j = i - 1;
		while ((j >= 0) && (xs[j] > key)) {
			xs[j + 1] = xs[j];
			j = j - 1;
		}
		xs[j + 1] = key;
	}

	for (size_t i = 0; i < 100; i++) {
		fprintf(output, "%ld\n", xs[i]);
	}
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
