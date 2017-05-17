#include <iostream>
#include <fstream>
#include "compiler.h"

using namespace std;

int main(int argc, char **argv) {

    /*if(argc < 3) {
        printf("Usage: program input_file output_file\n");
        return -1;
    }*/

    ifstream input(argv[1]);

    if (!input.is_open()) {
        fprintf(stderr, "Error opening file: %s\n", argv[1]);
        return -1;
    }

    FILE *output = fopen("asd.txt", "w");

    if (!output) {
        fprintf(stderr, "Error opening file\n");
        return -1;
    }

    process_input(input);
    first_run();
    second_run();
}