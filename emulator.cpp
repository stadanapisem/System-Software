#include <cstdio>
#include <ostream>
#include <fstream>
#include <map>
#include <regex>
#include <iostream>
#include "emulator.h"
#include "table.h"
#include "Relocation.h"
#include "instruction.h"

using namespace std;
using namespace inst;

#define SEC_DATA string, vector<unsigned char>

static vector<SymTableEntry> SymbolTable;
static vector<Relocation> Rels;
static map<SEC_DATA > Data;
static processor P;
static unsigned overall_size = IVT_SIZE + STACK_SIZE; // Space reserved for IVT and program stack
static string swap_file_path = "./swap_file";

int main(int argc, char **argv) {
    if (argc < 1) {
        printf("Usage: program input_file\n");
        return -1;
    }

    ifstream input(argv[1]);

    if (!input.is_open()) {
        fprintf(stderr, "Error opening file: %s\n", argv[1]);
        return -1;
    }

    load_file(input);
    if (!check_symbols()) {
        cerr << "There are undefined symbols! Error!" << endl;
        exit(-1);
    }

    if (!check_sections()) {
        cerr << "Sections overlap! Error!" << endl;
        exit(-1);
    }

    for (auto &i:Data) {
        if (i.first.substr(0, 5) == ".text")
            overall_size += i.second.size();
    }

    overall_size = (overall_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1)); // round up to muliple of PAGE_SIZE

    for (auto &i:Data) {
        if (i.first.substr(0, 5) == ".data")
            overall_size += i.second.size();
    }

    overall_size = (overall_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    for (auto &i:Data) {
        if (i.first.substr(0, 7) == ".rodata")
            overall_size += i.second.size();
    }

    overall_size = (overall_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    for (auto &i:SymbolTable) {
        if (i.name.substr(0, 4) == ".bss")
            overall_size += i.size;
    }

    overall_size = (overall_size + PAGE_SIZE - 1) / PAGE_SIZE; // Pages needed
    P.Pages = new page[overall_size];
    P.Pages[0].id = 0; //= {.id = 0, .start_adr = 0, .in_mem = -1, .flags = READ | WRITE | EXECUTE};
    P.Pages[0].start_adr = 0;
    P.Pages[0].in_mem = -1;
    P.Pages[0].flags = READ | WRITE | EXECUTE; // Page 0 should always be in memory, it contains IVT and stack
    P.next_free_page = 1;
    P.occupied[0] = 1;

    load_ivt(0);

    load_segments(START_SP + STACK_SIZE);

    update_symbols();

    update_relocations();

    while(1) {
        opcode_t op = read_inst();
        execute_instruction(op);
    }
}

regex section_name("^.(text|data|rodata|bss)(.[0-9]+)?$");

static unsigned getSectionSize(string name) {
    for (auto &i :SymbolTable)
        if (i.name == name)
            return i.size;
}

void load_file(ifstream &input) {
    string line;

    bool read_sym_table = false, read_rel = false, read_sec = false;
    string sec_name;
    unsigned size;

    while (getline(input, line)) {
        if (line == "#TabelaSimbola") {
            read_sym_table = true;

            continue;
        } else if (line.substr(0, 4) == "#rel") {
            read_sym_table = false;
            read_rel = true;

            continue;
        } else if (regex_match(line, section_name)) {
            read_sym_table = false;
            read_rel = false;
            read_sec = true;

            vector<unsigned char> data;
            size = getSectionSize(line);
            data.reserve(size);
            Data.insert(pair<SEC_DATA >(line, data));
            sec_name = line;

            continue;
        } else if (line == "#end")
            break;

        if (read_sym_table)
            SymbolTable.push_back(SymTableEntry(line));
        else if (read_rel)
            Rels.push_back(Relocation(line));
        else if (read_sec) {
            vector<unsigned char> &data = Data.find(sec_name)->second;

            for (int i = 0; i < 16 && size > 0; i++, size--) {
                unsigned char tmp = (unsigned short) line[i * 2];
                data.push_back((unsigned char) (tmp & 0xFF));
            }

        }
    }
}

bool check_symbols() {
    bool found = false;
    for (auto &i:SymbolTable)
        if (!i.ordinal_section_no)
            return false;
        else if (i.name == "START")
            found = true;


    return found;
}

bool check_sections() {
    for (auto &i:SymbolTable) {
        for (auto &j:SymbolTable) {
            if (i.ordinal_no != j.ordinal_no) {
                if (i.type == "SEG" && j.type == "SEG") {
                    if (i.start_adr != 0 && j.start_adr != 0 &&
                        (i.start_adr <= j.start_adr + j.size || j.start_adr <= i.size + i.start_adr))
                        return false;
                }
            }
        }
    }

    return true;
}

void load_ivt(unsigned page_id) {

}

static page find_swap_out_page(unsigned mem_idx) { // Find page which is in mem_idx place in memory
    for (int i = 0; i < overall_size; i++)
        if (P.Pages[i].in_mem = mem_idx)
            return P.Pages[i];
}

void page_fault(unsigned page_id) {
    string swap_tmp_path = "./tmp_swap";

    fstream input(swap_file_path, ios::binary | ios::in);
    fstream tmp_out(swap_tmp_path, ios::binary | ios::out);

    if (input.is_open() && tmp_out.is_open()) {
        swap_file_entry tmp;

        while (!input.eof()) {

            /*input.read((char *) &tmp.p.id, sizeof(tmp.p.id));
            input.read((char *) &tmp.p.start_adr, sizeof(tmp.p.start_adr));
            input.read((char *) &tmp.p.flags, sizeof(tmp.p.flags));
            input.read((char *) &tmp.p.in_mem, sizeof(tmp.p.in_mem));
            input.read((char *) &tmp.data, sizeof(tmp.data));*/
            input.read((char *) &tmp, sizeof(tmp));

            if (page_id == tmp.p.id) {
                /*input.close();

                fstream output(swap_file_path, ios::binary);
                swap_file_entry out; //= {.p = find_page_id(P.next_free_page), .data = P.mem[P.next_free_page]};
                //out.p = find_page_id(P.next_free_page);
                //out.data = P.mem[P.next_free_page];
                memcpy(out.data, P.mem[P.next_free_page], sizeof(out.data));
                output.write((char *) &out, sizeof(out));
                output.close();

                P.mem[P.next_free_page] = tmp.data;
                P.Pages[page_id] = tmp.p;
                P.Pages[page_id].in_mem = P.next_free_page;
                break;*/
                if (!P.occupied[P.next_free_page]) {
                    P.Pages[page_id] = tmp.p;
                    P.Pages[page_id].in_mem = P.next_free_page;
                    P.mem[P.next_free_page] = tmp.data; // TODO memcpy?
                    P.next_free_page = P.next_free_page % MAX_PAGES_IN_MEM + 1;
                    P.occupied[P.next_free_page] = 1;
                } else { // Izbaci postojecu pa ubaci novu
                    swap_file_entry rem;
                    rem.p = find_swap_out_page(P.next_free_page);
                    memcpy(rem.data, P.mem[P.next_free_page], sizeof(rem.data));
                    P.next_free_page = P.next_free_page % MAX_PAGES_IN_MEM + 1;
                    // TODO write to tmp file
                    tmp_out.write((char *) &rem, sizeof(rem));
                }
            } else {
                // TODO write to tmp file
                tmp_out.write((char *) &tmp, sizeof(tmp));
            }
        }

        input.close();
        tmp_out.close();

        remove(swap_file_path.c_str());
        rename(swap_tmp_path.c_str(), swap_file_path.c_str());
        remove(swap_tmp_path.c_str());

    } else {
        cerr << "Can't access swap_file!" << endl;
        exit(-1);
    }
}

bool mem_write(unsigned addr, unsigned val, unsigned flags) {
    unsigned page_id = addr >> 12;

    if (P.Pages[page_id].in_mem != -1) {
        if (P.Pages->flags & flags) {
            P.mem[P.Pages->in_mem][addr] = (unsigned char) (val & 0xFF);
            P.mem[P.Pages->in_mem][addr + 1] = (unsigned char) ((val >> 8) & 0xFF);
            P.mem[P.Pages->in_mem][addr + 2] = (unsigned char) ((val >> 16) & 0xFF);
            P.mem[P.Pages->in_mem][addr + 3] = (unsigned char) ((val >> 24) & 0xFF);
            return true;
        } else {
            cerr << "Not allowed to write here" << endl;
            exit(-1);
        }
    } else
        page_fault(page_id);

    return false;
}

bool mem_read(unsigned addr, unsigned &val, unsigned flags) {
    unsigned page_id = addr >> 12;

    if (P.Pages[page_id].in_mem != -1) {
        if (P.Pages->flags & flags) {
            val = P.mem[P.Pages->in_mem][addr + 3] << 24;
            val |= P.mem[P.Pages->in_mem][addr + 2] << 16;
            val |= P.mem[P.Pages->in_mem][addr + 1] << 8;
            val |= P.mem[P.Pages->in_mem][addr];

            return true;
        } else {
            cerr << "Not allowed to read here!" << endl;
            exit(-1);
        }
    } else
        page_fault(page_id);

    return false;
}

static void update_start_addr(string name, unsigned addr) {
    for (auto &i: SymbolTable)
        if (i.name == name)
            i.start_adr = addr;
}

static unsigned get_bss_size(string name) {
    for (auto &i:SymbolTable)
        if (i.name == name)
            return i.size;
}

void load_segments(unsigned addr) {
    fstream out(swap_file_path, ios::binary | ios::out);

    page * p = &P.Pages[addr >> 12];
    p->flags = READ | EXECUTE;
    p->start_adr = addr;
    p->id = addr >> 12;
    p->in_mem = -1;
    swap_file_entry sw;
    sw.p = *p;
    memset(sw.data, 0, sizeof(sw.data));
    int idx = 0;

    for (auto &i:Data) {

        if (i.first.substr(0, 5) == ".text") {
            update_start_addr(i.first, addr);

            for (int j = 0; j < i.second.size(); j++, addr++) {
                sw.data[idx++] = i.second[j];

                if (idx == PAGE_SIZE - 1) {
                    out.write((char *) &sw, sizeof(sw));
                    p = &P.Pages[addr >> 12];
                    p->flags = READ | EXECUTE;
                    p->start_adr = addr;
                    p->id = addr >> 12;
                    p->in_mem = -1;

                    sw.p = *p;
                    memset(sw.data, 0, sizeof(sw.data));
                    idx = 0;
                }
            }
        }
    }

    if (idx)
        out.write((char *) &sw, sizeof(sw));

    addr = (addr + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));


    p = &P.Pages[addr >> 12];
    p->flags = READ | WRITE;
    p->start_adr = addr;
    p->id = addr >> 12;
    p->in_mem = -1;
    sw.p = *p;
    memset(sw.data, 0, sizeof(sw.data));
    idx = 0;

    for (auto &i:Data) {
        if (i.first.substr(0, 5) == ".data") {
            update_start_addr(i.first, addr);

            for (int j = 0; j < i.second.size(); j++, addr++) {
                sw.data[idx++] = i.second[j];

                if (idx == PAGE_SIZE - 1) {
                    out.write((char *) &sw, sizeof(sw));
                    p = &P.Pages[addr >> 12];
                    p->flags = READ | WRITE;
                    p->start_adr = addr;
                    p->id = addr >> 12;
                    p->in_mem = -1;

                    sw.p = *p;
                    memset(sw.data, 0, sizeof(sw.data));
                    idx = 0;
                }
            }
        }
    }

    if (!idx)
        out.write((char *) &sw, sizeof(sw));

    addr = (addr + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    p = &P.Pages[addr >> 12];
    p->flags = READ;
    p->start_adr = addr;
    p->id = addr >> 12;
    p->in_mem = -1;
    sw.p = *p;
    memset(sw.data, 0, sizeof(sw.data));
    idx = 0;

    for (auto &i:Data) {
        if (i.first.substr(0, 7) == ".rodata") {
            update_start_addr(i.first, addr);

            for (int j = 0; j < i.second.size(); j++, addr++) {
                sw.data[idx++] = i.second[j];

                if (idx == PAGE_SIZE - 1) {
                    out.write((char *) &sw, sizeof(sw));
                    p = &P.Pages[addr >> 12];
                    p->flags = READ;
                    p->start_adr = addr;
                    p->id = addr >> 12;
                    p->in_mem = -1;

                    sw.p = *p;
                    memset(sw.data, 0, sizeof(sw.data));
                    idx = 0;
                }

            }
        }
    }

    if (!idx)
        out.write((char *) &sw, sizeof(sw));

    addr = (addr + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    p = &P.Pages[addr >> 12];
    p->flags = READ | WRITE;
    p->start_adr = addr;
    p->id = addr >> 12;
    p->in_mem = -1;
    sw.p = *p;
    idx = 0;

    for (auto &i:Data) {
        if (i.first.substr(0, 4) == ".bss") {
            update_start_addr(i.first, addr);
            unsigned size = get_bss_size(i.first);

            for (int j = 0; j < size; j++, addr++) {
                idx++;

                if (idx == PAGE_SIZE - 1) {
                    out.write((char *) &sw, sizeof(sw));
                    p = &P.Pages[addr >> 12];
                    p->flags = READ | WRITE;
                    p->start_adr = addr;
                    p->id = addr >> 12;
                    p->in_mem = -1;

                    sw.p = *p;
                    idx = 0;
                }
            }
        }
    }

    if (!idx)
        out.write((char *) &sw, sizeof(sw));

    out.flush();
    out.close();
}

void update_symbols() {

    for (auto &i:SymbolTable)
        if (i.type == "SYM")
            i.start_adr = i.value + SymbolTable[i.ordinal_section_no - 1].start_adr;
}

static unsigned find_symbol_start_addr(int ord) {
    for (auto &i : SymbolTable)
        if (i.ordinal_no == ord)
            return i.start_adr;
}

static unsigned find_section_start_addr(int ord) {
    for(auto& i:SymbolTable)
        if(i.ordinal_no == ord)
            return find_symbol_start_addr(i.ordinal_section_no);
}

void update_relocations() {

    for (auto &i:Rels)
        if (i.type == 'A')
            while (!mem_write(i.address + find_section_start_addr(i.ordinal_no), find_symbol_start_addr(i.ordinal_no), WRITE));
    //else if(i.type == 'R')

}

static void stack_push(unsigned val) {
    P.registers[SP] += 4;
    if(P.registers[SP] >= STACK_SIZE) {
        cerr << "STACK OVERFLOW!" << endl;
        exit(-1);
    }

    while(!mem_write(P.registers[SP], val, WRITE)); // Shouldn't generate page fault
}

static unsigned stack_pop() {
    if(P.registers[SP] < START_SP) {
        cerr << "STACK UNDERFLOW!" << endl;
        exit(-1);
    }

    unsigned ret;

    while(!mem_read(P.registers[SP], ret, READ));
    P.registers[SP] -= 4;

    return ret;
}

void execute_instruction(opcode_t opcode) { // TODO Need for psw flags?
    switch (opcode.first_word.op) {
        case 0x00: { // INT

            if(opcode.first_word.r0 == 0)
                exit(0);

            // TODO What to do with the rest of the entries?
            break;
        }

        case 0x01: {
            P.registers[PC] = stack_pop();
            break;
        }

        case 0x02: { // JMP arg
// TODO Absolute or relative jump? Relocations?
            if(opcode.first_word.adr_mode == REG_DIR)
                P.registers[PC] = P.registers[opcode.first_word.r0];
            else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned val;
                mem_read(opcode.second_word, val, READ);
                P.registers[PC] = val;
            } else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned  val;
                mem_read(opcode.first_word.r0, val, READ);
                P.registers[PC] = val;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned  val;
                mem_read(opcode.first_word.r1 + opcode.second_word, val, READ);
                P.registers[PC] = val;
            } else {
                cerr << "Unknown address mode!" << endl;
            }

            break;
        }

        case 0x03: { // CALL arg
            stack_push(P.registers[PC]);

            if(opcode.first_word.adr_mode == REG_DIR)
                P.registers[PC] = P.registers[opcode.first_word.r0];
            else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned val;
                mem_read(opcode.second_word, val, READ);
                P.registers[PC] = val;
            } else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned  val;
                mem_read(opcode.first_word.r0, val, READ);
                P.registers[PC] = val;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned  val;
                mem_read(opcode.first_word.r1 + opcode.second_word, val, READ);
                P.registers[PC] = val;
            } else {
                cerr << "Unknown address mode!" << endl;
            }

            break;
        }

        case 0x04: { // JZ reg0, arg

            unsigned arg;
            if(opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned val;
                mem_read(opcode.second_word, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned  val;
                mem_read(opcode.first_word.r1, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned  val;
                mem_read(opcode.first_word.r1 + opcode.second_word, val, READ);
                arg = val;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if(opcode.first_word.r0 == 0)
                P.registers[PC] = arg;

            break;
        }

        case 0x05: { // JNZ reg0, arg
            unsigned arg;
            if(opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned val;
                mem_read(opcode.second_word, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned  val;
                mem_read(opcode.first_word.r1, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned  val;
                mem_read(opcode.first_word.r1 + opcode.second_word, val, READ);
                arg = val;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if(opcode.first_word.r0 != 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x06: { // JGZ reg0, arg
            unsigned arg;
            if(opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned val;
                mem_read(opcode.second_word, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned  val;
                mem_read(opcode.first_word.r1, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned  val;
                mem_read(opcode.first_word.r1 + opcode.second_word, val, READ);
                arg = val;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if(((int) opcode.first_word.r0) > 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x07: { // JGEZ reg0, arg
            unsigned arg;
            if(opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned val;
                mem_read(opcode.second_word, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned  val;
                mem_read(opcode.first_word.r1, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned  val;
                mem_read(opcode.first_word.r1 + opcode.second_word, val, READ);
                arg = val;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if(((int) opcode.first_word.r0) >= 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x08: { // JLZ reg0, arg
            unsigned arg;
            if(opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned val;
                mem_read(opcode.second_word, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned  val;
                mem_read(opcode.first_word.r1, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned  val;
                mem_read(opcode.first_word.r1 + opcode.second_word, val, READ);
                arg = val;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if(((int) opcode.first_word.r0) < 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x09: { // JLEZ reg0, arg
            unsigned arg;
            if(opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned val;
                mem_read(opcode.second_word, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned  val;
                mem_read(opcode.first_word.r1, val, READ);
                arg = val;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned  val;
                mem_read(opcode.first_word.r1 + opcode.second_word, val, READ);
                arg = val;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if(((int) opcode.first_word.r0) <= 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x10: { // LOAD reg0, arg
            int arg;
            if(opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned tmp;
                mem_read(opcode.first_word.r1, tmp, READ);
                arg = (int) tmp;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned tmp;
                mem_read(opcode.first_word.r1 + opcode.second_word, tmp, READ);
                arg = (int) tmp;
            } else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned tmp;
                mem_read(opcode.second_word, tmp, READ);
                arg = (int) tmp;
            } else if (opcode.first_word.adr_mode == IMM)
                arg = opcode.second_word;

            // TODO Proper Sign Extention
            if(opcode.first_word.type == UW)
                P.registers[opcode.first_word.r0] = (unsigned) (arg & 0xFFFF);
            else if(opcode.first_word.type == SW)
                P.registers[opcode.first_word.r0] = arg & 0xFFFF;
            else if(opcode.first_word.type == UB)
                P.registers[opcode.first_word.r0] = (unsigned short) (arg & 0xFF);
            else if(opcode.first_word.type == SB)
                P.registers[opcode.first_word.r0] = (short) (arg & 0xFF);
            else
                P.registers[opcode.first_word.r0] = arg;

            break;
        }

        case 0x11: { // STORE reg0, arg
            unsigned arg;
            if(opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if(opcode.first_word.adr_mode == REG_IND) {
                unsigned tmp;
                mem_read(opcode.first_word.r1, tmp, READ);
                arg = (int) tmp;
            } else if(opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned tmp;
                mem_read(opcode.first_word.r1 + opcode.second_word, tmp, READ);
                arg = (int) tmp;
            } else if(opcode.first_word.adr_mode == MEM_DIR) {
                unsigned tmp;
                mem_read(opcode.second_word, tmp, READ);
                arg = (int) tmp;
            } else if (opcode.first_word.adr_mode == IMM)
                arg = opcode.second_word;

            // TODO Proper Sign Extention
            if(opcode.first_word.type == UB)
                mem_write(arg, P.registers[opcode.first_word.r0] & 0xFF, WRITE);
            else if(opcode.first_word.type == SW)
                mem_write(arg, P.registers[opcode.first_word.r0] & 0xFFFF, WRITE);
            else
                mem_write(arg, P.registers[opcode.first_word.r0], WRITE);

            break;
        }

        case 0x20: { // PUSH reg0
            stack_push(opcode.first_word.r0);
            break;
        }

        case 0x21: { // POP reg0
            P.registers[opcode.first_word.r0] = stack_pop();
            break;
        }

        case 0x30: { // ADD reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] + P.registers[opcode.first_word.r2];
            break;
        }

        case 0x31: { // SUB reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] - P.registers[opcode.first_word.r2];
            break;
        }

        case 0x32: { // MUL reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] * P.registers[opcode.first_word.r2];
            break;
        }

        case 0x33: { // DIV reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] / P.registers[opcode.first_word.r2];
            break;
        }

        case 0x34: { // MOD reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] % P.registers[opcode.first_word.r2];
            break;
        }

        case 0x35: { // AND reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] & P.registers[opcode.first_word.r2];
            break;
        }

        case 0x36: { // OR reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] | P.registers[opcode.first_word.r2];
            break;
        }

        case 0x37: { // XOR reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] ^ P.registers[opcode.first_word.r2];
            break;
        }

        case 0x38: { // NOT reg0, reg1
            P.registers[opcode.first_word.r0] = ~P.registers[opcode.first_word.r1];
            break;
        }

        case 0x39: { // ASL reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] << P.registers[opcode.first_word.r2];
            break;
        }

        case 0x3A: { // ASR reg0, reg1, reg2
            P.registers[opcode.first_word.r0] = P.registers[opcode.first_word.r1] >> P.registers[opcode.first_word.r2];
            break;
        }
    }
}