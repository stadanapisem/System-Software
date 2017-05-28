#ifndef CODE_COMPILER_H
#define CODE_COMPILER_H

#include <cstdio>
#include <fstream>
#include <regex>
#include "table.h"

using namespace std;

enum token_t {
    LABEL, SYMBOL, DIRECTIVE, SECTION, INSTRUCTION, OPR_DEC, OPR_HEX,
    OPR_REG_DIR, OPR_REG_IND, OPR_REG_IND_OFF, OPR_IMM,
    OPR_REG_IND_DOLLAR, OPR_MEM_DIR, ILLEGAL
};

enum flags_t {
    LOCAL = 0x0, GLOBAL = 0x1, EXTERN = 0x2,
    READ = 0x4, WRITE = 0x8, EXECUTE = 0x10
};

struct match_t {
    token_t type;
    regex pattern;
};

extern bool second_pass_check;
extern vector<SymTableEntry> Symbol_Table;
extern unsigned offset;
extern string current_section;
extern vector<match_t> Matcher;

void process_input(std::ifstream &);

void first_pass();

void second_pass();

token_t find_match(string token);

token_t find_address_mode(string token);

unsigned getOperandValue(string token);

int find_section_ord(string name);

void write_obj(ofstream &);

#endif //CODE_COMPILER_H
