#include "Relocation.h"
#include <iostream>

using namespace std;

vector<Relocation> Relocations;

Relocation::Relocation(string in) {
    sscanf(in.c_str(), "%x %c %d", &address, &type, &ordinal_no);
}

ostream& operator << (ostream& out, const Relocation& rel) {
    out << "0x" << hex << rel.address << " " << rel.type << " " << rel.ordinal_no;

    return out;
}

std::vector<Section> Sectionlist;

void Section::write(int val, unsigned size) {
    if(this->name.substr(0, 4) == ".bss") {
        cerr << "Writing in bss section not allowed" << endl;
        exit(-1);
    }

    if(size == 1) {
        data[counter++] = (unsigned char) (val & 0xFF);
    } else if(size == 2) {
        data[counter++] = (unsigned char) (val & 0xFF);
        data[counter++] = (unsigned char) ((val >> 8) & 0xFF);
    } else if(size == 4) {
        data[counter++] = (unsigned char) (val & 0xFF);
        data[counter++] = (unsigned char) ((val >> 8) & 0xFF);
        data[counter++] = (unsigned char) ((val >> 16) & 0xFF);
        data[counter++] = (unsigned char) ((val >> 24) & 0xFF);
    } else {
        cerr << "Neki Err" << endl;
        exit(-1);
    }
}

void Section::decrease(int val) {
    this->counter -= val;
}

ostream& operator << (ostream& out, const Section& s) {

    for(int i = 1; i <= s.counter; i++) {
        out << hex << s.data[i - 1];

        if(i % 16 == 0)
            out << endl;
        else
            out << " ";
    }

    return out;
}