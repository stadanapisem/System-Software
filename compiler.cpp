#include <string>
#include <vector>
#include <cstring>
#include <regex>
#include <iostream>
#include <queue>
#include "compiler.h"
#include "table.h"
#include "instruction.h"

using namespace std;

int SymTableEntry::num = 1;
vector<vector<string> > Tokens;
vector<SymTableEntry> Symbol_Table;
//vector<Section> Sections;

vector<match_t> Matcher = {
        {.type = LABEL, .pattern = regex("^([a-zA-Z_][a-zA-Z0-9]{0,}):$")},
        {.type = SYMBOL, .pattern = regex("^([a-zA-Z_])([a-zA-Z0-9])*$")},
        {.type = DIRECTIVE, .pattern = regex("^(def|DUP|ORG|DD|DW|DB|.global)$")},
        {.type = SECTION, .pattern = regex("^.(text|data|rodata|bss)(.[0-9]+)?$")},
        {.type = INSTRUCTION, .pattern = regex(
                "^(int|jmp|call|ret|jz|jnz|jgz|jgez|jlz|jlez|load|store|push|pop|add|sub|mul|div|mod|and|or|xor|not|asl|asr)$")},
        {.type = OPR_DEC, .pattern = regex("^[0-9]+$")},
        {.type = OPR_HEX, .pattern = regex("^0x[0-9a-fA-F]+$")},
        {.type = OPR_REG_DIR, .pattern = regex("^(r([0-9]+)|(pc|sp))$")},
        {.type = OPR_REG_IND, .pattern = regex("^\\[((r[0-9]+)|(pc|sp))\\]$")},
        {.type = OPR_REG_IND_OFF, .pattern = regex(
                "^\\[((r([0-9]+)|(sp|pc))\\+((0x[0-9a-fA-F]+)|([0-9]+)|(([a-zA-Z_])([a-zA-Z0-9])*)))\\]$")},
        {.type = OPR_IMM, .pattern = regex("^#((0x[0-9a-fA-F]+)|([0-9]+)|(([a-zA-Z_])([a-zA-Z0-9])*))$")},
        {.type = OPR_REG_IND_DOLLAR, .pattern = regex("\\$((0x[0-9a-fA-F]+)|([0-9]+)|(([a-zA-Z_])([a-zA-Z0-9])*))$")},
        {.type = OPR_MEM_DIR, .pattern = regex("((^[a-zA-Z_])([a-zA-Z0-9])*$)|(^[0-9]+$)|(^0x[0-9a-fA-F]+$)")}
};

vector<string> tokenize(string &s, const char *delim) {
    vector<string> ret;
    char *token = strtok((char *) s.c_str(), delim);
    while (token) {
        ret.push_back(string(token));
        token = strtok(NULL, delim);
    }

    return ret;
}

void process_input(ifstream &input) {
    string line;

    while (getline(input, line)) {
        size_t position = line.find(';');

        if (position != string::npos)
            line = line.substr(0, position);

        vector<string> tokens = tokenize(line, " ,\t\n");

        if (!tokens.size())
            continue;

        if (tokens[0] == ".end")
            break;

        Tokens.push_back(tokens);
    }
}

static string toLower(string x) {
    string lower = "";
    lower.reserve(x.size());
    for (int i = 0; i < x.size(); i++)
        lower += tolower(x[i]);

    return lower;
}

token_t find_match(string token) {
    token_t ret = ILLEGAL;

    if (regex_match(token, Matcher[SYMBOL].pattern))
        ret = SYMBOL;

    //std::transform(token.begin(), token.end(), token.begin(), ::tolower);
    string lower = toLower(token);

    for (int i = 0; i < 5; i++) {
        if (Matcher[i].type == SYMBOL)
            continue;

        if (regex_match(token, Matcher[i].pattern) || regex_match(lower, Matcher[i].pattern)) {
            if (ret == SYMBOL || ret == ILLEGAL)
                ret = Matcher[i].type;
            else {
                cerr << "ERROR" << endl;
                exit(-1);
            }
        }

    }

    return ret;
}

token_t find_address_mode(string token) {
    string lower = toLower(token);

    for (int i = 5; i < Matcher.size(); i++)
        if (regex_match(token, Matcher[i].pattern) || regex_match(lower, Matcher[i].pattern))
            return Matcher[i].type;

    return ILLEGAL;
}

static bool find_symbol(string name) {
    for (auto &i : Symbol_Table)
        if (i.name == name)
            return true;

    return false;
}

static int find_section_ord(string name) {
    for (auto &i: Symbol_Table)
        if (i.type == "SEG" && i.name == name)
            return i.ordinal_no;

    return -1;
}

static void add_symbol(string type, string name, string section, unsigned val, flags_t f) {

    int sec_ord = find_section_ord(section);

    if (sec_ord == -1) {
        cerr << "Can't add symbol " << name << " into section " << section << endl;
        exit(-1);
    }

    if (find_symbol(name)) {
        cerr << "Symbol already in table" << endl;
        exit(-1);
    }

    Symbol_Table.push_back(SymTableEntry(type, name, sec_ord, val, f));
}

static void add_section(string type, string name, unsigned start, unsigned size, unsigned f) {
    Symbol_Table.push_back(SymTableEntry(type, name, -1, start, size, f));
}

static section_name_t toSectionName(string name) {
    if (name == "NONE")
        return NONE;
    if (name == ".bss")
        return BSS;
    if (name == ".rodata")
        return RODATA;
    if (name == ".data")
        return DATA;
    if (name == ".text")
        return TEXT;
}

unsigned getOperandValue(string token) {
    unsigned ret;

    if (regex_match(token, Matcher[OPR_DEC].pattern)) {
        sscanf(token.c_str(), "%u", &ret);
        return ret;
    } else if (regex_match(token, Matcher[OPR_HEX].pattern)) {
        sscanf(token.c_str(), "%x", &ret);
        return ret;
    }

}

void first_run() {

    unsigned offset = 0;
    string current_section = "none";
    bool was_org = false;

    for (auto &i : Tokens) {
        queue<string> token_line;

        for (auto &token : i)
            token_line.push(token);

        int state_machine = 0;
        while (!token_line.empty()) {

            if (state_machine == 0) {
                string current_token = token_line.front();
                token_t current_token_type = find_match(current_token);

                if (current_token_type == LABEL) {
                    token_line.pop();
                    add_symbol("SYM", current_token.substr(0, current_token.size() - 1), current_section, offset,
                               LOCAL);
                }

                state_machine = 1;
            } else if (state_machine == 1) {
                string current_token = token_line.front();
                token_line.pop();
                token_t current_token_type = find_match(current_token);

                if (was_org && current_token_type != SECTION) {
                    cerr << "An section must follow ORG directive" << endl;
                    exit(-1);
                }

                switch (current_token_type) {
                    case SECTION: {
                        if (current_section != "none") {
                            for (auto &sym : Symbol_Table)
                                if (sym.name == current_section)
                                    sym.size = offset - sym.start_adr;
                        }

                        current_section = current_token; // TODO FLAGS
                        add_section("SEG", current_token, offset, 0, (unsigned) (READ | WRITE | EXECUTE | LOCAL));

                        if (was_org)
                            was_org = false;

                        state_machine = 2;
                        break;
                    }
                    case DIRECTIVE: {

                        if (current_token == ".global") { // Worry about in second pass
                            while (!token_line.empty())
                                token_line.pop();

                            break;
                        }

                        string new_token = token_line.front();
                        token_line.pop();
                        token_t new_token_type = find_address_mode(new_token);

                        if (new_token_type != OPR_DEC && new_token_type != OPR_HEX && new_token_type != OPR_IMM) {
                            cerr << "This token not ok " << new_token << endl;
                            exit(-1);
                        }

                        unsigned value = getOperandValue(new_token), bytes = 0;

                        if (current_token == "DB")
                            bytes = 1;
                        else if (current_token == "DW")
                            bytes = 2;
                        else if (current_token == "DD") {
                            bytes = 4;
                        }

                        offset += bytes;

                        if (current_token == "ORG") {
                            offset = value;
                            was_org = true;
                        }

                        state_machine = 2;
                        break;
                    }

                    case INSTRUCTION: {
                        string mnemonic = toLower(current_token);
                        auto inst = Instructions.find(mnemonic);
                        if (inst == Instructions.end()) {
                            cerr << "Unknown instruction " << current_token << endl;
                            break;//exit(-1);
                        }

                        inst::opcode_t opcode = inst->second(token_line);
                        if(opcode.using_both)
                            offset += 8;
                        else offset += 4;
                        while (!token_line.empty())
                            token_line.pop();

                        break;
                    }

                    default:
                        cerr << current_token + " Can't process that!" << endl;
                        exit(-1);
                }
            } else if (state_machine == 2) {
                if (!token_line.empty()) {
                    cerr << "Didn't parse whole line" << endl;
                    exit(-1);
                }

                state_machine = 1;
            }
        }
    }

    //Sections.push_back(Section(current_section, offset));
    for (auto &sym : Symbol_Table)
        if (sym.name == current_section)
            sym.size = offset - sym.start_adr;

    //for (auto &i : Sections)
    //    cout << i.name << " " << i.size << endl;
    //cout << "------------------------" << endl;
    for (auto &i : Symbol_Table)
        cout << i << endl;
}

void second_run() {

    unsigned offset = 0;
    string current_section = "none";

    for (auto &i : Tokens) {
        queue<string> token_line;

        for (auto &token : i)
            token_line.push(token);

        int state_machine = 0;

        while (!token_line.empty()) {
            if (state_machine == 0) {
                string current_token = token_line.front();
                token_t current_token_type = find_match(current_token);

                if (current_token_type == LABEL)
                    token_line.pop();

                state_machine = 1;

            } else if (state_machine == 1) {

                string current_token = token_line.front();
                token_line.pop();
                token_t current_token_type = find_match(current_token);

                switch (current_token_type) {
                    case SECTION: {
                        current_section = current_token;
                        state_machine = 2;
                        break;
                    }

                    case DIRECTIVE: {
                        if (current_token == ".global")
                            state_machine = 3;


                        break;
                    }

                    case INSTRUCTION: {
                        string mnemonic = toLower(current_token);
                        auto inst = Instructions.find(mnemonic);
                        if (inst == Instructions.end()) {
                            cerr << "Unknown instruction " << current_token << endl;
                            break;//exit(-1);
                        }

                        inst::opcode_t opcode = inst->second(token_line);
                        break;
                    }

                    default:
                        cerr << current_token + " Can't process that!" << endl;
                        //exit(-1);

                }
            } else if (state_machine == 2) {
                if (!token_line.empty()) {
                    cerr << "Didn't parse whole line" << endl;
                    exit(-1);
                }

                state_machine = 1;
            } else if (state_machine == 3) { // .global
                if (token_line.empty())
                    state_machine = 2;
                else {
                    while (!token_line.empty()) {
                        string current_token = token_line.front();
                        token_line.pop();
                        bool has = false;

                        for (auto &i :Symbol_Table)
                            if (i.name == current_token)
                                i.flags = GLOBAL, has = true;

                        if (!has)
                            add_symbol("SYM", current_token, current_section, offset, EXTERN);
                    }
                }
            }
        }
    }

}