#ifndef CODE_RELOCATION_H
#define CODE_RELOCATION_H


#include <vector>
#include <string>
#include <cstring>
#include <ostream>

class Relocation {
public:
    unsigned address;
    char type;
    int ordinal_section_no, ordinal_no;

    Relocation(unsigned a, char t, int sec, int sim) :
            address(a), type(t), ordinal_section_no(sec), ordinal_no(sim) {}

    friend std::ostream&operator << (std::ostream&, const Relocation&);
};

extern std::vector<Relocation> Relocations;

class Section {
public:
    unsigned char * data; unsigned counter;
    std::string name;
    bool fixed_addres;
    unsigned start_addres;

    Section(std::string n, unsigned addr, bool isorg, unsigned size) : name(n), start_addres(addr), fixed_addres(isorg) {
        data = new unsigned char[size];
        memset(data, 0, sizeof(data));
        counter = 0;
    }

    void write(int val, unsigned size);
    void decrease(int val);

    friend std::ostream& operator << (std::ostream& out, const Section& s);
};

extern std::vector<Section> Sectionlist;

#endif //CODE_RELOCATION_H
