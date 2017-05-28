#include "Relocation.h"
#include <iostream>

using namespace std;

vector<Relocation> Relocations;

ostream& operator << (ostream& out, const Relocation& rel) {
    out << "0x" << hex << rel.address << " " << rel.type << " " << rel.ordinal_no;

    return out;
}

std::vector<Section> Sectionlist;

void Section::write(int val, unsigned size) {
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

    for(int i = 0; i < s.counter; i++) {
        out << hex << s.data[i];

        if(i != 0 && i % 16 == 0)
            out << endl;
        else
            out << " ";
    }

    return out;
}