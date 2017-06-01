#ifndef CODE_SECTION_H
#define CODE_SECTION_H

#include <string>
#include <iomanip>
#include <cstdio>
#include <cstring>


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

    SymTableEntry(string t) {
        char type[3];
        sscanf(t.c_str(), "%s", type);
        this->type = string(type);

        if(this->type == "SYM") {
            char name[123], f;
            sscanf(t.c_str(), "SYM %d %s %d %x %c", &ordinal_no, name, &ordinal_section_no, &value, &f);

            this->name = string(name);
            this->flags = (f == 'G') ? 0x1 : (f == 'L' ? 0x0 : 0x2);
        } else {
            char name[123], f[10];
            sscanf(t.c_str(), "SEG %d %s %d %x %x %s", &ordinal_no, name, &ordinal_section_no, &start_adr, &size, f);

            this->name = string(name);
            size_t len = strlen(f);
            for(int i = 0; i < len; i++) {
                switch (f[i]) {
                    case 'G':
                        this->flags |= 0x1;
                        break;
                    case 'L':
                        this->flags |= 0x1;
                        break;
                    case 'R':
                        this->flags |= 0x4;
                        break;
                    case 'W':
                        this->flags |= 0x8;
                        break;
                    case 'E':
                        this->flags |= 0x10;
                        break;
                    case 'F':
                        this->flags |= 0x20;
                        break;
                }
            }
        }
    }

    friend ostream& operator << (ostream& out, const SymTableEntry& t) {
        if(t.type == "SYM") {
            out << "SYM" << " " << t.ordinal_no << " " << t.name
                << " " << t.ordinal_section_no << " 0x" << hex
                << t.value << " ";

            if(t.flags == 0x1)
                out << 'G';
            else if(t.flags == 0x00)
                out << 'L';

        } else {
            out << "SEG" << " " << t.ordinal_no << " " << t.name
                << " " << t.ordinal_section_no << " " << "0x" << hex
                << t.start_adr << " " << "0x" << hex << t.size << " " << 'L';

            if(t.flags & 0x4)
                out << 'R';
            if(t.flags & 0x8)
                out << 'W';
            if(t.flags & 0x10)
                out << "X";
        }

        return out;
    }

};

#endif
