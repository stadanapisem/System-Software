#ifndef CODE_EMULATOR_H
#define CODE_EMULATOR_H

#include <fstream>
#include "instruction.h"

using namespace std;

// 4KB, 4GB
#define PAGE_SIZE (4096)
#define ADDRESS_SPACE (4096 * 1024 * 1024)
#define MAX_PAGES_IN_MEM 5

#define IVT_SIZE (32 * 4)
#define STACK_SIZE (4096 - 32 * 4)
#define START_SP (32 * 4)

#define READ 0x4
#define WRITE 0x8
#define EXECUTE 0x10

enum REGS {
    PC = 0x11, SP = 0x10
};

struct page {
    unsigned id, start_adr, flags; // in mem is -1 if page on disk
    int in_mem;                    // otherwise mem[in_mem]
};

struct __attribute__((packed)) swap_file_entry {
    page p;
    unsigned char data[PAGE_SIZE];
};

void load_file(ifstream&);

bool check_symbols();

bool check_sections();

void load_segments(unsigned);

void load_ivt(unsigned);

void update_symbols();

void update_relocations();

void execute_instruction(inst::opcode_t opcode);

struct processor {
    unsigned registers[18];
    const unsigned IVTP = 0;

    struct {
        unsigned I:1, L:2, :25, N:1, Z:1, C:1, V:1;
    } PSW;

    page * Pages; // Tabela stranica 4KB * 1024 (4096B)
    unsigned char * mem[MAX_PAGES_IN_MEM];
    bool occupied[MAX_PAGES_IN_MEM];
    unsigned next_free_page;

};
#endif //CODE_EMULATOR_H
