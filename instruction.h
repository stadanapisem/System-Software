#ifndef CODE_INSTRUCTION_H
#define CODE_INSTRUCTION_H

#include <string>
#include <queue>
#include "compiler.h"

using namespace std;

namespace inst {
    struct opcode_t {
        struct {
            unsigned op:8, adr_mode:3, r0:5, r1:5, r2:5, type:3, unused:2;
        } first_word;

        unsigned second_word;
        bool using_both;
    };

    enum Address_mode_codes {
        IMM = 0x4, REG_DIR = 0x0, REG_IND = 0x2, REG_IND_OFF = 0x7, MEM_DIR = 0x6
    };

    enum REGS {
        SP = 0x10, PC = 0x11
    };

    enum DATA_TYPE {
        DW = 0x00, UW = 0x01, SW = 0x5, UB = 0x3, SB = 0x7
    };
}
extern map<string, inst::opcode_t(*)(queue<string>&)> Instructions;

#endif //CODE_INSTRUCTION_H
