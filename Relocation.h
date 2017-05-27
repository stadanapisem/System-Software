#ifndef CODE_RELOCATION_H
#define CODE_RELOCATION_H


#include <vector>

class Relocation {
public:
    static int num;
    unsigned address;
    char type;
    int ordinal_section_no, ordinal_no;

    Relocation(unsigned a, char t, int sec) :
            address(a), type(t), ordinal_section_no(sec), ordinal_no(num++) {}

};

extern std::vector<Relocation> Relocations;

#endif //CODE_RELOCATION_H
