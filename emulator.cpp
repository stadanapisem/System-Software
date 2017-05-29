#include <cstdio>
#include <ostream>
#include <fstream>
#include <map>
#include <regex>
#include <iostream>
#include "emulator.h"
#include "table.h"
#include "Relocation.h"

using namespace std;

#define SEC_DATA string, vector<unsigned char>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: program input_file output_file\n");
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

    if(!check_sections()) {
        cerr << "Sections overlap! Error!" << endl;
        exit(-1);
    }

}

static vector<SymTableEntry> SymbolTable;
static vector<Relocation> Rels;
static map<SEC_DATA > Data;

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

    while (getline(input, line)) {
        if (line == "#TabelaSimbola")
            read_sym_table = true;
        else if (line.substr(0, 4) == "#rel") {
            read_sym_table = false;
            read_rel = true;
        } else if (regex_match(line, section_name)) {
            read_sym_table = false;
            read_rel = false;
            read_sec = true;

            vector<unsigned char> data;
            data.reserve(getSectionSize(line));
            Data.insert(pair<SEC_DATA >(line, data));
            sec_name = line;
        } else if (line == "#end")
            break;

        if (read_sym_table)
            SymbolTable.push_back(SymTableEntry(line));
        else if (read_rel)
            Rels.push_back(Relocation(line));
        else if (read_sec) {
            vector<unsigned char> data = Data.find(sec_name)->second;
            unsigned x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16;
            sscanf(line.c_str(), "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
                   &x1, &x2, &x3, &x4, &x5, &x6, &x7, &x8, &x9, &x10, &x11, &x12, &x13, &x14, &x15, &x16);

            data.push_back((unsigned char) x1);
            data.push_back((unsigned char) x2);
            data.push_back((unsigned char) x3);
            data.push_back((unsigned char) x4);
            data.push_back((unsigned char) x5);
            data.push_back((unsigned char) x6);
            data.push_back((unsigned char) x7);
            data.push_back((unsigned char) x8);
            data.push_back((unsigned char) x9);
            data.push_back((unsigned char) x10);
            data.push_back((unsigned char) x11);
            data.push_back((unsigned char) x12);
            data.push_back((unsigned char) x13);
            data.push_back((unsigned char) x14);
            data.push_back((unsigned char) x15);
            data.push_back((unsigned char) x16);
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
    for(auto & i:SymbolTable) {
        for(auto & j:SymbolTable) {
            if(i.ordinal_no != j.ordinal_no) {
                if(i.type == "SEG" && j.type == "SEG") {
                    if(i.start_adr != 0 && j.start_adr != 0 && (i.start_adr <= j.start_adr + j.size || j.start_adr <= i.size + i.start_adr))
                        return false;
                }
            }
        }
    }

    return true;
}