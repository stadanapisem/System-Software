#include <iostream>
#include <fstream>
#include "compiler.h"

using namespace std;

int main(int argc, char **argv) {

    if(argc < 3) {
        printf("Usage: program input_file output_file\n");
        return -1;
    }

    ifstream input(argv[1]);

    if (!input.is_open()) {
        fprintf(stderr, "Error opening file: %s\n", argv[1]);
        return -1;
    }

    ofstream output(argv[2]);

    if (!output.is_open()) {
        fprintf(stderr, "Error opening file\n");
        return -1;
    }

    process_input(input);
    first_pass();
    second_pass();
    write_obj(output);
}
