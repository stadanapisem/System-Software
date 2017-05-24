#include "instruction.h"
#include <map>
#include <iostream>

using namespace inst;

static unsigned operand_value(string token, opcode_t& code) {
    if(regex_match(token, Matcher[OPR_REG_DIR].pattern)) {
        code.first_word.adr_mode = REG_DIR;

        if(token[0] == 'p' || token[0] == 'P')
            return PC;
        else if(token[0] == 's' || token[0] == 'S')
            return SP;
        else {
            unsigned ret;
            sscanf(token.c_str(), "r%u", &ret);
            return ret;
        }
    } else if(regex_match(token, Matcher[OPR_REG_IND].pattern)) {
        code.first_word.adr_mode = REG_IND;

        if(token[1] == 'p' || token[1] == 'P')
            return PC;
        else if(token[1] == 's' || token[1] == 'S')
            return SP;
        else {
            unsigned ret;
            sscanf(token.c_str(), "r%u", &ret);
            return ret;
        }
    } else if(regex_match(token, Matcher[OPR_REG_IND_OFF].pattern)) {
        code.first_word.adr_mode = REG_IND_OFF;
        code.using_both = true;
        string tmp = token;
        tmp = tmp.substr(tmp.find('+') + 1, token.size() - 1);
        tmp = tmp.substr(0, tmp.size() - 1);
        code.second_word = getOperandValue(tmp);

        if(token[1] == 'p' || token[1] == 'P')
            return PC;
        else if(token[1] == 's' || token[1] == 'S')
            return SP;
        else {
            unsigned ret;
            sscanf(token.c_str(), "[r%u", &ret);
            return ret;
        }
    } else if(regex_match(token, Matcher[OPR_REG_IND_DOLLAR].pattern)) {
        code.first_word.adr_mode = REG_IND_OFF;
        code.first_word.r0 = PC;
        code.using_both = true;
        return getOperandValue(token.substr(1));
        // TODO token == SYMBOL or address
    } else if(regex_match(token, Matcher[OPR_IMM].pattern)) {
        code.first_word.adr_mode = IMM;
        code.using_both = true;
        return getOperandValue(token.substr(1));
    } else if(regex_match(token, Matcher[OPR_MEM_DIR].pattern)) {
        code.first_word.adr_mode = MEM_DIR;
        code.using_both = true;

        // TODO token == SYMBOL or address
    }
}

static unsigned parse_operand(queue<string>& tokens, vector<token_t> modes, opcode_t& code) {
    if(tokens.empty()) {
        cerr << "Can't parse: no operands!" << endl;
        exit(-1);
    }

    string current_token = tokens.front(); tokens.pop();
    token_t current_token_type = find_address_mode(current_token);

    for(auto&mode : modes)
        if(current_token_type == mode) {
            return operand_value(current_token, code);
        }

    cerr << "Wrong operand\n" << current_token << '\t' << current_token_type << endl;
    exit(-1);
}

opcode_t instructionINT(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.using_both = false;
    ret.first_word.op = 0x00;
    ret.first_word.r1 = 0;
    ret.first_word.r2 = 0;
    ret.first_word.type = 0;
    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionJMP(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR, OPR_REG_IND, OPR_REG_IND_OFF, OPR_REG_IND_DOLLAR, OPR_MEM_DIR, OPR_DEC, OPR_HEX};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x02;

    unsigned op_value = parse_operand(tokens, address_modes, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r0 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r0 != 0)
                break;

            ret.first_word.r0 = op_value;
            break;
    }

    return ret;
}

opcode_t instructionCALL(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR, OPR_REG_IND, OPR_REG_IND_OFF, OPR_REG_IND_DOLLAR, OPR_MEM_DIR, OPR_HEX, OPR_DEC};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x03;
    unsigned op_value = parse_operand(tokens, address_modes, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r0 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r0 != 0)
                break;

            ret.first_word.r0 = op_value;
            break;
    }

    return ret;
}

opcode_t instructionRET(queue<string>& tokens) {
    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x01;
    return ret;
}

opcode_t instructionJZ(queue<string>& tokens) {
    vector<token_t> address_modes_op = {OPR_MEM_DIR, OPR_REG_IND, OPR_REG_IND_DOLLAR, OPR_REG_IND_OFF, OPR_DEC, OPR_HEX};
    vector<token_t> address_modes_reg = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x04;

    ret.first_word.r0 = parse_operand(tokens, address_modes_reg, ret);
    unsigned op_value = parse_operand(tokens, address_modes_op, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r1 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r1 != 0)
                break;

            ret.first_word.r1 = op_value;
            break;
        case MEM_DIR:
            ret.second_word = op_value;
    }

    return ret;
}

opcode_t instructionJNZ(queue<string>& tokens) {
    vector<token_t> address_modes_op = {OPR_MEM_DIR, OPR_REG_IND, OPR_REG_IND_DOLLAR, OPR_REG_IND_OFF, OPR_DEC, OPR_HEX};
    vector<token_t> address_modes_reg = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x05;

    ret.first_word.r0 = parse_operand(tokens, address_modes_reg, ret);
    unsigned op_value = parse_operand(tokens, address_modes_op, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r1 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r1 != 0)
                break;

            ret.first_word.r1 = op_value;
            break;
        case MEM_DIR:
            ret.second_word = op_value;
    }

    return ret;
}

opcode_t instructionJGZ(queue<string>& tokens) {
    vector<token_t> address_modes_op = {OPR_MEM_DIR, OPR_REG_IND, OPR_REG_IND_DOLLAR, OPR_REG_IND_OFF, OPR_DEC, OPR_HEX};
    vector<token_t> address_modes_reg = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x06;

    ret.first_word.r0 = parse_operand(tokens, address_modes_reg, ret);
    unsigned op_value = parse_operand(tokens, address_modes_op, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r1 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r1 != 0)
                break;

            ret.first_word.r1 = op_value;
            break;
        case MEM_DIR:
            ret.second_word = op_value;
    }

    return ret;
}

opcode_t instructionJGEZ(queue<string>& tokens) {
    vector<token_t> address_modes_op = {OPR_MEM_DIR, OPR_REG_IND, OPR_REG_IND_DOLLAR, OPR_REG_IND_OFF, OPR_DEC, OPR_HEX};
    vector<token_t> address_modes_reg = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x07;

    ret.first_word.r0 = parse_operand(tokens, address_modes_reg, ret);
    unsigned op_value = parse_operand(tokens, address_modes_op, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r1 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r1 != 0)
                break;

            ret.first_word.r1 = op_value;
            break;
        case MEM_DIR:
            ret.second_word = op_value;
    }

    return ret;
}

opcode_t instructionJLZ(queue<string>& tokens) {
    vector<token_t> address_modes_op = {OPR_MEM_DIR, OPR_REG_IND, OPR_REG_IND_DOLLAR, OPR_REG_IND_OFF, OPR_DEC, OPR_HEX};
    vector<token_t> address_modes_reg = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x08;

    ret.first_word.r0 = parse_operand(tokens, address_modes_reg, ret);
    unsigned op_value = parse_operand(tokens, address_modes_op, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r1 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r1 != 0)
                break;

            ret.first_word.r1 = op_value;
            break;
        case MEM_DIR:
            ret.second_word = op_value;
    }

    return ret;
}

opcode_t instructionJLEZ(queue<string>& tokens) {
    vector<token_t> address_modes_op = {OPR_MEM_DIR, OPR_REG_IND, OPR_REG_IND_DOLLAR, OPR_REG_IND_OFF, OPR_DEC, OPR_HEX};
    vector<token_t> address_modes_reg = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x09;

    ret.first_word.r0 = parse_operand(tokens, address_modes_reg, ret);
    unsigned op_value = parse_operand(tokens, address_modes_op, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r1 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r1 != 0)
                break;

            ret.first_word.r1 = op_value;
            break;
        case MEM_DIR:
            ret.second_word = op_value;
    }

    return ret;
}

opcode_t instructionLOAD(queue<string>& tokens) {
    vector<token_t> address_modes_op = {OPR_MEM_DIR, OPR_REG_IND, OPR_REG_IND_DOLLAR, OPR_REG_IND_OFF, OPR_DEC, OPR_HEX, OPR_IMM, OPR_REG_DIR};
    vector<token_t> address_modes_reg = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x10;

    ret.first_word.r0 = parse_operand(tokens, address_modes_reg, ret);
    unsigned op_value = parse_operand(tokens, address_modes_op, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r1 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r1 != 0)
                break;

            ret.first_word.r1 = op_value;
            break;
        case MEM_DIR:
            ret.second_word = op_value;
            break;
        case IMM:
            ret.second_word = op_value; // TODO EXTEND ACCORDING TO THE SUFFIX
            break;
    }

    return ret;
}

opcode_t instructionSTORE(queue<string>& tokens) {
    vector<token_t> address_modes_op = {OPR_MEM_DIR, OPR_REG_IND, OPR_REG_IND_DOLLAR, OPR_REG_IND_OFF, OPR_REG_DIR};
    vector<token_t> address_modes_reg = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x11;

    ret.first_word.r0 = parse_operand(tokens, address_modes_reg, ret);
    unsigned op_value = parse_operand(tokens, address_modes_op, ret);

    switch(ret.first_word.adr_mode) {
        case REG_DIR:
        case REG_IND:
            ret.first_word.r1 = op_value;
            break;
        case REG_IND_OFF:
            if(ret.first_word.r1 != 0)
                break;

            ret.first_word.r1 = op_value;
            break;
        case MEM_DIR:
            ret.second_word = op_value;
            break;
        case IMM:
            ret.second_word = op_value; // TODO EXTEND ACCORDING TO THE SUFFIX
            break;
    }

    return ret;
}

opcode_t instructionPUSH(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x20;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionPOP(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x21;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionADD(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x30;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionSUB(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x31;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionMUL(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x32;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionDIV(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x33;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionMOD(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x34;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionAND(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x35;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionOR(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x36;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionXOR(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x37;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionNOT(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x38;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionASL(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x39;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

opcode_t instructionASR(queue<string>& tokens) {
    vector<token_t> address_modes = {OPR_REG_DIR};

    opcode_t ret;
    memset(&ret, 0, sizeof(ret));

    ret.first_word.op = 0x3A;

    ret.first_word.r0 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r1 = parse_operand(tokens, address_modes, ret);
    ret.first_word.r2 = parse_operand(tokens, address_modes, ret);

    return ret;
}

map<string, opcode_t (*)(queue<string>&)> Instructions = {
        {.first = "int", .second =  instructionINT},
        {.first = "jmp", .second = instructionJMP},
        {.first = "call", .second = instructionCALL},
        {.first = "ret", .second = instructionRET},
        {.first = "jz", .second = instructionJZ},
        {.first = "jnz", .second = instructionJNZ},
        {.first = "jgz", .second = instructionJGZ},
        {.first = "jgez", .second = instructionJGEZ},
        {.first = "jlz", .second = instructionJLZ},
        {.first = "jlez", .second = instructionJLEZ},
        {.first = "load", .second = instructionLOAD},
        {.first = "store", .second = instructionSTORE},
        {.first = "push", .second = instructionPUSH},
        {.first = "pop", .second = instructionPOP},
        {.first = "add", .second = instructionADD},
        {.first = "sub", .second = instructionSUB},
        {.first = "mul", .second = instructionMUL},
        {.first = "div", .second = instructionDIV},
        {.first = "mod", .second = instructionMOD},
        {.first = "and", .second = instructionAND},
        {.first = "or", .second = instructionOR},
        {.first = "xor", .second = instructionXOR},
        {.first = "not", .second = instructionNOT},
        {.first = "asl", .second = instructionASL},
        {.first = "asr", .second = instructionASR}
};