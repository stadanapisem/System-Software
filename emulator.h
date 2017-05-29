#ifndef CODE_EMULATOR_H
#define CODE_EMULATOR_H

#include <fstream>

using namespace std;


enum REGS {
    PC = 0x11, SP = 0x10
};

struct page {
    unsigned id, start_adr, flags, in_mem; // in mem is 0 if page on disk
                                           // otherwise mem[in_mem]
};

void load_file(ifstream&);

bool check_symbols();

bool check_sections();

struct processor {
    unsigned registers[18];

    struct {
        unsigned I:1, L:2, :25, N:1, Z:1, C:1, V:1;
    } PSW;

    page Pages[1024]; // Tabela stranica
    unsigned char ** mem;

};
#endif //CODE_EMULATOR_H
