#ifndef CODE_EMULATOR_H
#define CODE_EMULATOR_H

#include <fstream>
#include "instruction.h"

using namespace std;

// 4KB, 4GB
#define PAGE_SIZE (4096)
#define ADDRESS_SPACE (4096 * 1024 * 1024)
#define MAX_PAGES_IN_MEM 5

#define OUT_REGISTER_ADDR 32
#define IN_REGISTER_ADDR 36
#define IVT_SIZE (32 * 4 + 8)
#define STACK_SIZE (4096 - 32 * 4 - 8)
#define START_SP (32 * 4 + 8)

#define READ 0x4
#define WRITE 0x8
#define EXECUTE 0x10

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

unsigned load_ivt();

void update_symbols();

void update_relocations();

void execute_instruction(inst::opcode_t opcode);

void handle_interrupts();

inst::opcode_t read_inst();

bool mem_read(unsigned addr, unsigned &val, unsigned flags);

bool mem_write(unsigned addr, unsigned val, unsigned flags);

void keyboard_thread();

static void stack_push(unsigned val);

struct processor {
    unsigned registers[18];
    const unsigned IVTP = 0;

    struct {
        unsigned I:1, L:2, :25, N:1, Z:1, C:1, V:1;
    } PSW;

    page* Pages; // Tabela stranica 4KB * 1024 (4096B)
    unsigned char mem[MAX_PAGES_IN_MEM][PAGE_SIZE];
    bool occupied[MAX_PAGES_IN_MEM];
    unsigned next_free_page;
    bool run;

};
#endif //CODE_EMULATOR_H
