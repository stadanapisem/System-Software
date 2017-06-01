
#include <iostream>
#include <regex>
#include <chrono>
#include <thread>
#include <assert.h>
#include "emulator.h"
#include "Relocation.h"
#include "table.h"

using namespace inst;

#define SEC_DATA string, vector<unsigned char>

static vector<SymTableEntry> SymbolTable;
static vector<Relocation> Rels;
static map<SEC_DATA > Data;
static processor P;
static unsigned overall_size = IVT_SIZE + STACK_SIZE + PAGE_SIZE; // Space reserved for IVT and program stack
static string swap_file_path = "./swap_file";
static volatile char key_chr, key_int = false;
typedef chrono::high_resolution_clock Clock;
static auto timer = Clock::now();
volatile bool stop_thread = false;


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
    input.close();
    if (!check_symbols()) {
        cerr << "There are undefined symbols! Error!" << endl;
        exit(-1);
    }

    if (!check_sections()) {
        cerr << "Sections overlap! Error!" << endl;
        exit(-1);
    }

    for (auto &i:SymbolTable) {
        if (i.name.substr(0, 5) == ".text")
            overall_size += i.size + i.start_adr;
    }

    overall_size = (overall_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1)); // round up to muliple of PAGE_SIZE

    for (auto &i:SymbolTable) {
        if (i.name.substr(0, 5) == ".data")
            overall_size += i.size + i.start_adr;
    }

    overall_size = (overall_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    for (auto &i:SymbolTable) {
        if (i.name.substr(0, 7) == ".rodata")
            overall_size += i.size + i.start_adr;
    }

    overall_size = (overall_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    for (auto &i:SymbolTable) {
        if (i.name.substr(0, 4) == ".bss")
            overall_size += i.size + i.start_adr;
    }

    overall_size = (overall_size + PAGE_SIZE - 1) / PAGE_SIZE; // Pages needed
    P.Pages = new page[overall_size + 1];
    for (int i = 0; i < overall_size + 1; i++) {
        P.Pages[i].id = i; //= {.id = 0, .start_adr = 0, .in_mem = -1, .flags = READ | WRITE | EXECUTE};
        P.Pages[i].start_adr = 0;
        P.Pages[i].in_mem = -1;
        P.Pages[i].flags = READ | WRITE | EXECUTE; // Page 0 should always be in memory, it contains IVT and stack
    }

    P.next_free_page = 1;
    P.occupied[0] = 1;

    unsigned addr = load_ivt();

    load_segments(addr);

    update_symbols();

    update_relocations();

    for (auto &i:SymbolTable)
        if (i.name == "START") {
            P.registers[PC] = i.start_adr;
            break;
        }

    thread key_thr(keyboard_thread);

    P.registers[SP] = START_SP;
    P.run = true;
    P.PSW.I = 0; // Not masking interrupts
    P.PSW.L = 0; // User mode

    timer = Clock::now();
    while (P.run) {
        opcode_t op = read_inst();
        execute_instruction(op);
        handle_interrupts();
    }

    cerr << "exiting...";
    stop_thread = true;
    key_thr.detach();
    remove(swap_file_path.c_str());
    delete[] P.Pages;
    //terminate();
    return 0;
}

void keyboard_thread() {
    while (!stop_thread) {
        while (key_int);

        char c;
        cin >> noskipws >> c;
        key_chr = c;
        key_int = true;
    }
}

void handle_interrupts() {

    if (P.PSW.I == 1) // No interrupt nesting
        return;

    // Timer interrupt has bigger priority than the keyboard one
    if (chrono::duration_cast<chrono::milliseconds>(Clock::now() - timer).count() > 100) { //
        timer = Clock::now();
        stack_push(P.registers[PC]);
        unsigned val;
        while (!mem_read(P.IVTP + 4 * 4, val, READ));
        P.registers[PC] = val;
        P.PSW.I = 1;
        return;
    } else if (key_int) {
        key_int = false;
        mem_write(IN_REGISTER_ADDR, key_chr, WRITE);

        stack_push(P.registers[PC]);
        unsigned val;
        while (!mem_read(P.IVTP + 4 * 5, val, READ));
        P.registers[PC] = val;
        P.PSW.I = 1;
        return;
    }

    unsigned val;
    while (!mem_read(OUT_REGISTER_ADDR, val, READ));
    if (val) {
        char c = (char) (val & 0xFF);
        while(!mem_write(OUT_REGISTER_ADDR, 0, WRITE));
        cout << c;
        cout.flush();
    }
}

regex section_name("^.(text|data|rodata|bss)(.[0-9]+)?$");

static unsigned getSectionSize(string name) {
    for (auto &i :SymbolTable)
        if (i.name == name)
            return i.size;
}

static unsigned get_section_ord(string name) {
    for (auto &i:SymbolTable)
        if (i.name == name)
            return i.ordinal_no;
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
            sec_name = line.substr(4);
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

        if (read_sym_table) {
            SymbolTable.push_back(SymTableEntry(line));
            if ((SymbolTable[SymbolTable.size() - 1].flags & 0x20) &&
                (SymbolTable[SymbolTable.size() - 1].start_adr < PAGE_SIZE)) {
                cerr << "Section " << SymbolTable[SymbolTable.size() - 1].name << " can not be placed there" << endl;
                exit(-1);
            }

        } else if (read_rel) {
            Rels.push_back(Relocation(line));
            Rels[Rels.size() - 1].ordinal_section_no = get_section_ord(sec_name);
        } else if (read_sec) {
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

unsigned load_ivt() {
    unsigned char data_sec[] = {0, 0x10, 0x0, 0x0, 0xc, 0x10, 0x0, 0x0, 0x10, 0x10, 0x0, 0x0, 0x14, 0x10, 0x0, 0x0,
                                0x20, 0x10, 0x0, 0x0, 0x24, 0x10, 0x0, 0x0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0};

    unsigned char text_sec[] = {0, 0, 144, 16, 136, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0,
                                0, 0, 1, 0, 0, 192, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                0, 0, 1, 0, 0, 192, 16, 36, 0, 0, 0, 64, 8, 1, 55, 0,
                                0, 193, 17, 36, 0, 0, 0, 0, 0, 0, 1};

    page *p = &P.Pages[0];
    p->start_adr = 0;
    p->id = 0;
    p->flags = READ | WRITE;
    p->in_mem = 0;

    memcpy(P.mem[0], data_sec, sizeof(data_sec));

    p = &P.Pages[1];
    p->id = 1;
    p->start_adr = PAGE_SIZE;
    p->flags = READ | EXECUTE;
    p->in_mem = -1;

    fstream out(swap_file_path, ios::binary | ios::app);
    swap_file_entry sw;
    sw.p = *p;
    memcpy(sw.data, text_sec, sizeof(text_sec));

    out.write((char *) &sw, sizeof(sw));
    out.flush();
    out.close();
    return 2 * PAGE_SIZE;
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

        while (input.read((char *) &tmp, sizeof(tmp))) {

            if (page_id == tmp.p.id) {
                if (!P.occupied[P.next_free_page]) {
                    P.Pages[page_id] = tmp.p;
                    P.Pages[page_id].in_mem = P.next_free_page;
                    memcpy(P.mem[P.next_free_page], tmp.data, PAGE_SIZE);
                    P.occupied[P.next_free_page] = 1;

                    if (P.next_free_page == 4)
                        P.next_free_page = 1;
                    else P.next_free_page++;

                } else { // Izbaci postojecu pa ubaci novu
                    swap_file_entry rem;
                    rem.p = find_swap_out_page(P.next_free_page);
                    memcpy(rem.data, P.mem[P.next_free_page], sizeof(rem.data));
                    P.next_free_page = P.next_free_page % MAX_PAGES_IN_MEM + 1;
                    tmp_out.write((char *) &rem, sizeof(rem));
                }
            } else {
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
    assert(page_id <= overall_size);

    if (P.Pages[page_id].in_mem != -1) {
        if (P.Pages[page_id].flags & flags) {
            addr = addr & (PAGE_SIZE - 1); // Only offset
            P.mem[P.Pages[page_id].in_mem][addr] = (unsigned char) (val & 0xFF);
            P.mem[P.Pages[page_id].in_mem][addr + 1] = (unsigned char) ((val >> 8) & 0xFF);
            P.mem[P.Pages[page_id].in_mem][addr + 2] = (unsigned char) ((val >> 16) & 0xFF);
            P.mem[P.Pages[page_id].in_mem][addr + 3] = (unsigned char) ((val >> 24) & 0xFF);
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
    assert(page_id <= overall_size);

    if (P.Pages[page_id].in_mem != -1) {
        if (P.Pages[page_id].flags & flags) {
            addr = addr & (PAGE_SIZE - 1); // Only offset
            val = P.mem[P.Pages[page_id].in_mem][addr + 3] << 24;
            val |= P.mem[P.Pages[page_id].in_mem][addr + 2] << 16;
            val |= P.mem[P.Pages[page_id].in_mem][addr + 1] << 8;
            val |= P.mem[P.Pages[page_id].in_mem][addr];

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

unsigned load_segment(unsigned addr, int pos, string name, unsigned flags, bool fixed) {
    fstream out(swap_file_path, ios::binary | ios::app);

    page *p = &P.Pages[addr >> 12];
    p->flags = flags;
    p->start_adr = (addr >> 12) * PAGE_SIZE;
    p->id = addr >> 12;
    p->in_mem = -1;
    swap_file_entry sw;
    sw.p = *p;

    int idx = 0;
    if (name != ".bss")
        memset(sw.data, 0, sizeof(sw.data));
    else
        for (auto &i:SymbolTable)
            if (i.name == name)
                idx += i.size;


    for (auto &i:Data) {

        if ((i.first == name) && fixed || (i.first.substr(0, pos) == name && !fixed)) {
            update_start_addr(i.first, addr);
            fixed = false;

            for (int j = 0; j < i.second.size(); j++, addr++) {
                sw.data[idx++] = i.second[j];

                if (idx == PAGE_SIZE - 1) {
                    out.write((char *) &sw, sizeof(sw));
                    p = &P.Pages[addr >> 12];
                    p->flags = flags;
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

    out.flush();
    out.close();
    return ((addr + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1)));
}

void load_segments(unsigned addr) {
    bool segments_loaded[4] = {0, 0, 0, 0};

    vector<pair<string, unsigned>> V;
    for (auto &i:SymbolTable)
        if (i.flags & 0x20)
            V.push_back(pair<string, unsigned>(i.name, i.start_adr));


    sort(V.begin(), V.end());


    for (auto &i : V) {
        if (i.first.substr(0, 5) == ".text" && !segments_loaded[0]) {
            addr = load_segment(i.second, 5, i.first, READ | EXECUTE, 1);
            segments_loaded[0] = 1;
        } else if (i.first.substr(0, 5) == ".data" && !segments_loaded[1]) {
            addr = load_segment(i.second, 5, i.first, READ | WRITE, 1);
            segments_loaded[1] = 1;
        } else if (i.first.substr(0, 7) == ".rodata" && !segments_loaded[2]) {
            addr = load_segment(i.second, 7, i.first, READ, 1);
            segments_loaded[2] = 1;
        } else if (i.first.substr(0, 4) == ".bss" && !segments_loaded[3]) {
            addr = load_segment(i.second, 4, i.first, READ | WRITE, 1);
            segments_loaded[3] = 1;
        }
    }

    if (!segments_loaded[0]) {
        addr = load_segment(addr, 5, ".text", READ | EXECUTE, 0);
        segments_loaded[0] = 1;
    }

    if (!segments_loaded[1]) {
        addr = load_segment(addr, 5, ".data", READ | WRITE, 0);
        segments_loaded[0] = 1;
    }

    if (!segments_loaded[2]) {
        addr = load_segment(addr, 7, ".rodata", READ, 0);
        segments_loaded[2] = 1;
    }

    if (!segments_loaded[3]) {
        addr = load_segment(addr, 4, ".bss", READ | WRITE, 0);
        segments_loaded[3] = 1;
    }
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
    for (auto &i:SymbolTable)
        if (i.ordinal_no == ord)
            return find_symbol_start_addr(i.ordinal_section_no);
}

void update_relocations() {

    for (auto &i:Rels)
        if (i.type == 'A') {
            unsigned tmp;
            while (!mem_read(i.address + find_symbol_start_addr(i.ordinal_section_no), tmp, READ));
            //cerr << tmp << endl;
            while (!mem_write(i.address + find_symbol_start_addr(i.ordinal_section_no),
                              find_symbol_start_addr(i.ordinal_no) + tmp,
                              WRITE | READ | EXECUTE));
        } else if (i.type == 'R') {
            unsigned byte_address = i.address + find_section_start_addr(i.ordinal_no);
            while (!mem_write(byte_address, find_symbol_start_addr(i.ordinal_no) - (byte_address + 8),
                              WRITE | READ | EXECUTE));
        }

}

static void stack_push(unsigned val) {
    P.registers[SP] += 4;
    if (P.registers[SP] >= STACK_SIZE) {
        cerr << "STACK OVERFLOW!" << endl;
        exit(-1);
    }

    while (!mem_write(P.registers[SP], val, WRITE)); // Shouldn't generate page fault, but just to be safe
}

static unsigned stack_pop() {
    if (P.registers[SP] < START_SP) {
        cerr << "STACK UNDERFLOW!" << endl;
        exit(-1);
    }

    unsigned ret;

    while (!mem_read(P.registers[SP], ret, READ));
    P.registers[SP] -= 4;

    return ret;
}

opcode_t read_inst() {
    opcode_t ret;

    unsigned tmp_first, tmp_second;

    while (!mem_read(P.registers[PC], tmp_first, EXECUTE));
    P.registers[PC] += 4;

    ret.set_first_word(tmp_first);
    if (ret.first_word.adr_mode == IMM || ret.first_word.adr_mode == MEM_DIR ||
        ret.first_word.adr_mode == REG_IND_OFF) {
        while (!mem_read(P.registers[PC], tmp_second, EXECUTE));
        P.registers[PC] += 4;
        ret.second_word = tmp_second;
    }

    return ret;
}

unsigned sign_extent(unsigned val, unsigned bits) {
    unsigned sig_bit = val && (1 << (bits - 1));
    for (int i = bits; i < 32; i++)
        val |= sig_bit << i;

    return val;
}

void execute_instruction(opcode_t opcode) { // TODO Need for psw flags?
    switch (opcode.first_word.op) {
        case 0x00: { // INT

            if (P.registers[opcode.first_word.r0] == 0)
                P.run = false;
            else if (P.registers[opcode.first_word.r0] < 33) {
                stack_push(P.registers[PC]);
                unsigned val;
                while (!mem_read(P.IVTP + 4 * P.registers[opcode.first_word.r0], val, READ));
                P.registers[PC] = val;
                P.PSW.I = 1;
            } else {
                cerr << "Unknown interrupt entry" << endl;
                exit(-1);
            }

            break;
        }

        case 0x01: {
            P.registers[PC] = stack_pop();
            P.PSW.I = 0;
            break;
        }

        case 0x02: { // JMP arg
            if (opcode.first_word.adr_mode == REG_DIR)
                P.registers[PC] = P.registers[opcode.first_word.r0];
            else if (opcode.first_word.adr_mode == MEM_DIR) {
                //unsigned val;
                //while (!mem_read(opcode.second_word, val, READ));
                P.registers[PC] = opcode.second_word;
            } else if (opcode.first_word.adr_mode == REG_IND) {
                //while (!mem_read(P.registers[opcode.first_word.r0], val, READ));
                P.registers[PC] = P.registers[opcode.first_word.r0];
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
               // while (!mem_read(P.registers[opcode.first_word.r1] + opcode.second_word, val, READ));
                P.registers[PC] = P.registers[opcode.first_word.r1] + opcode.second_word;
            } else if (opcode.first_word.adr_mode == IMM) {
                P.registers[PC] = opcode.second_word;
            } else {
                cerr << "Unknown address mode!" << endl;
            }

            break;
        }

        case 0x03: { // CALL arg
            stack_push(P.registers[PC]);

            if (opcode.first_word.adr_mode == REG_DIR)
                P.registers[PC] = P.registers[opcode.first_word.r0];
            else if (opcode.first_word.adr_mode == MEM_DIR) {
                P.registers[PC] = opcode.second_word;
            } else if (opcode.first_word.adr_mode == REG_IND) {
                P.registers[PC] = P.registers[opcode.first_word.r0];
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
                P.registers[PC] = P.registers[opcode.first_word.r1] + opcode.second_word;
            } else {
                cerr << "Unknown address mode!" << endl;
            }

            break;
        }

        case 0x04: { // JZ reg0, arg

            unsigned arg;
            if (opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if (opcode.first_word.adr_mode == MEM_DIR) {
                arg = opcode.second_word;
            } else if (opcode.first_word.adr_mode == REG_IND) {
                arg = opcode.first_word.r1;
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
                arg = opcode.second_word;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if (P.registers[opcode.first_word.r0] == 0)
                P.registers[PC] = arg;

            break;
        }

        case 0x05: { // JNZ reg0, arg
            unsigned arg;
            if (opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if (opcode.first_word.adr_mode == MEM_DIR) {
                arg = opcode.second_word;
            } else if (opcode.first_word.adr_mode == REG_IND) {
                arg = opcode.first_word.r1;
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
                arg = opcode.second_word;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if (P.registers[opcode.first_word.r0] != 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x06: { // JGZ reg0, arg
            unsigned arg;
            if (opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if (opcode.first_word.adr_mode == MEM_DIR) {
                arg = opcode.second_word;
            } else if (opcode.first_word.adr_mode == REG_IND) {
                arg = opcode.first_word.r1;
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
                arg = opcode.second_word;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if (((int) P.registers[opcode.first_word.r0]) > 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x07: { // JGEZ reg0, arg
            unsigned arg;
            if (opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if (opcode.first_word.adr_mode == MEM_DIR) {
                arg = opcode.second_word;
            } else if (opcode.first_word.adr_mode == REG_IND) {
                arg = opcode.first_word.r1;
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
                arg = opcode.second_word;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if (((int) P.registers[opcode.first_word.r0]) >= 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x08: { // JLZ reg0, arg
            unsigned arg;
            if (opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if (opcode.first_word.adr_mode == MEM_DIR) {
                arg = opcode.second_word;
            } else if (opcode.first_word.adr_mode == REG_IND) {
                arg = opcode.first_word.r1;
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
                arg = opcode.second_word;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if (((int) P.registers[opcode.first_word.r0]) < 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x09: { // JLEZ reg0, arg
            unsigned arg;
            if (opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if (opcode.first_word.adr_mode == MEM_DIR) {
                arg = opcode.second_word;
            } else if (opcode.first_word.adr_mode == REG_IND) {
                arg = opcode.first_word.r1;
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
                arg = opcode.second_word;
            } else {
                cerr << "Unknown address mode!" << endl;
                exit(-1);
            }

            if (((int) P.registers[opcode.first_word.r0]) <= 0)
                P.registers[PC] = arg;
            break;
        }

        case 0x10: { // LOAD reg0, arg
            int arg;
            if (opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if (opcode.first_word.adr_mode == REG_IND) {
                unsigned tmp;
                while (!mem_read(P.registers[opcode.first_word.r1], tmp, READ));
                arg = (int) tmp;
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned tmp;
                while (!mem_read(P.registers[opcode.first_word.r1] + opcode.second_word, tmp, READ));
                arg = (int) tmp;
            } else if (opcode.first_word.adr_mode == MEM_DIR) {
                unsigned tmp;
                while (!mem_read(opcode.second_word, tmp, READ));
                arg = (int) tmp;
            } else if (opcode.first_word.adr_mode == IMM)
                arg = opcode.second_word;


            if (opcode.first_word.type == UW)
                P.registers[opcode.first_word.r0] = (unsigned) (arg & 0xFFFF);
            else if (opcode.first_word.type == SW)
                P.registers[opcode.first_word.r0] = sign_extent(arg, 16);
            else if (opcode.first_word.type == UB)
                P.registers[opcode.first_word.r0] = (unsigned short) (arg & 0xFF);
            else if (opcode.first_word.type == SB)
                P.registers[opcode.first_word.r0] = sign_extent(arg, 8);
            else
                P.registers[opcode.first_word.r0] = arg;

            break;
        }

        case 0x11: { // STORE reg0, arg
            unsigned arg;
            if (opcode.first_word.adr_mode == REG_DIR)
                arg = P.registers[opcode.first_word.r1];
            else if (opcode.first_word.adr_mode == REG_IND) {
                unsigned tmp;
                while (!mem_read(P.registers[opcode.first_word.r1], tmp, READ));
                arg = (int) tmp;
            } else if (opcode.first_word.adr_mode == REG_IND_OFF) {
                unsigned tmp;
                while (!mem_read(P.registers[opcode.first_word.r1] + opcode.second_word, tmp, READ));
                arg = (int) tmp;
            } else if (opcode.first_word.adr_mode == MEM_DIR) {
                arg = opcode.second_word;
            } else if (opcode.first_word.adr_mode == IMM)
                arg = opcode.second_word;


            if (opcode.first_word.type == UB)
                while (!mem_write(arg, P.registers[opcode.first_word.r0] & 0xFF, WRITE));
            else if (opcode.first_word.type == SW)
                while (!mem_write(arg, sign_extent(P.registers[opcode.first_word.r0], 16), WRITE));
            else
                while (!mem_write(arg, P.registers[opcode.first_word.r0], WRITE));

            break;
        }

        case 0x20: { // PUSH reg0
            stack_push(P.registers[opcode.first_word.r0]);
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