#ifndef CODE_COMPILER_H
#define CODE_COMPILER_H

#include <cstdio>
#include <fstream>
#include <regex>

using namespace std;

enum token_t {
    LABEL, SYMBOL, DIRECTIVE, SECTION, INSTRUCTION, OPR_DEC, OPR_HEX,
    ILLEGAL
};

enum SectionName {
    NONE, BSS, RODATA, DATA, TEXT
};

enum scope_t {
    LOCAL, GLOBAL, EXTERN
};

struct Section {
    string name;
    int size;

    Section(string a, int b) : name(a), size(b) {}
};

struct Match {
    token_t type;
    regex pattern;
};

struct SymTableEntry {
    string name;
    string section;
    unsigned offset;
    scope_t scope;
    int size;
};

void process_input(std::ifstream &);

void first_run();

void second_run();

#endif //CODE_COMPILER_H
