#ifndef CODE_COMPILER_H
#define CODE_COMPILER_H

#include <cstdio>
#include <fstream>
#include <regex>

using namespace std;

enum token_t {
    LABEL, SYMBOL, DIRECTIVE, SECTION, INSTRUCTION, OPR_DEC, OPR_HEX,
    OPR_REG_DIR, OPR_REG_IND, OPR_REG_IND_OFF, OPR_IMM,
    OPR_REG_IND_DOLLAR, OPR_MEM_DIR, ILLEGAL
};

enum section_name_t {
    NONE, BSS, RODATA, DATA, TEXT
};

enum flags_t {
    LOCAL, GLOBAL, EXTERN, READ, WRITE, EXECUTE
};

struct match_t {
    token_t type;
    regex pattern;
};

void process_input(std::ifstream &);

void first_run();

void second_run();

token_t find_match(string token);

extern vector<match_t> Matcher;

extern token_t find_address_mode(string token);

extern unsigned getOperandValue(string token);
#endif //CODE_COMPILER_H
