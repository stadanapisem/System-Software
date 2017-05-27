#ifndef CODE_SECTION_H
#define CODE_SECTION_H

#include <string>
using namespace std;

class SymTableEntry {
public:
    static int num;

    int ordinal_no, ordinal_section_no;
    string name, type;
    int value;
    unsigned size, start_adr, flags;


    SymTableEntry(string t, string n, int ord_sec, unsigned start, unsigned size, unsigned flag)
            : type(t), ordinal_no(num++), name(n), ordinal_section_no(ord_sec), start_adr(start), size(size), flags(flag) {

        if(ord_sec == -1)
            ordinal_section_no = ordinal_no;
    }

    SymTableEntry(string t, string n, int ord_sec, int value, unsigned flag)
            : type(t), ordinal_no(num++), name(n), ordinal_section_no(ord_sec), value(value), flags(flag) {

    }

    friend ostream& operator << (ostream& out, const SymTableEntry& t) {
        if(t.type == "SYM")
            out << "SYM | " << t.ordinal_no << '\t' << t.name << '\t' << t.ordinal_section_no << '\t' << t.value << '\t' << t.flags;
        else
            out << "SEG | " << t.ordinal_no << '\t' << t.name << '\t' << t.start_adr << '\t' << t.size << '\t' << t.flags;
    }

};

#endif
