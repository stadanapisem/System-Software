#include <string>
#include <vector>
#include <cstring>
#include <regex>
#include <iostream>
#include <queue>
#include "compiler.h"
#include "instruction.h"
#include "Relocation.h"

using namespace std;

int SymTableEntry::num = 1;
vector<vector<string> > Tokens;
vector<SymTableEntry> Symbol_Table;
unsigned offset;
string current_section;

vector<match_t> Matcher = {
        {.type = LABEL, .pattern = regex("^([a-zA-Z_][a-zA-Z0-9]{0,}):$")},
        {.type = SYMBOL, .pattern = regex("^([a-zA-Z_])([a-zA-Z0-9])*$")},
        {.type = DIRECTIVE, .pattern = regex("^(def|DUP|ORG|DD|DW|DB|.global)$")},
        {.type = SECTION, .pattern = regex("^.(text|data|rodata|bss)(.[0-9]+)?$")},
        {.type = INSTRUCTION, .pattern = regex(
                "^(int|jmp|call|ret|jz|jnz|jgz|jgez|jlz|jlez|load|store|push|pop|add|sub|mul|div|mod|and|or|xor|not|asl|asr)$")},
        {.type = OPR_DEC, .pattern = regex("^[0-9]+$")},
        {.type = OPR_HEX, .pattern = regex("^0x[0-9a-fA-F]+$")},
        {.type = OPR_REG_DIR, .pattern = regex("^((r|R)([0-9]+)|(pc|sp|SP|PC))$")},
        {.type = OPR_REG_IND, .pattern = regex("^\\[(((r|R)[0-9]+)|(pc|sp|PC|SP))\\]$")},
        {.type = OPR_REG_IND_OFF, .pattern = regex("^\\[(((r|R)([0-9]+)|(sp|pc|SP|PC))\\+(.*))\\]$")},
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

int find_section_ord(string name) {
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

    for(auto& i:Symbol_Table)
        if(i.name == name && i.ordinal_section_no == 0) {
            i.ordinal_section_no = find_section_ord(section);
            i.value = val;
            i.flags = f;
        } else if(i.name == name && i.ordinal_section_no != 0) {
            cerr << "Symbol already exists!" << endl;
            exit(-1);
        }

    Symbol_Table.push_back(SymTableEntry(type, name, sec_ord, val, f));
}

static void add_section(string type, string name, unsigned start, unsigned size, unsigned f) {
    Symbol_Table.push_back(SymTableEntry(type, name, -1, start, size, f));
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

    cerr << "Can't get this operands value!" << endl;
    exit(-1);
}

static unsigned get_flags(string section) {
    section = section.substr(1);
    section = section.substr(0, section.find('.'));

    if(section == "text")
        return (LOCAL | READ | EXECUTE);
    else if(section == "bss")
        return (LOCAL | READ | WRITE);
    else if(section == "rodata")
        return (LOCAL | READ);
    else if(section == "data")
        return (LOCAL | READ | WRITE);
}

void first_pass() {

    offset = 0;
    current_section = "none";
    unsigned was_org = 0;

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
                string current_token = token_line.front(); token_line.pop();
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

                        current_section = current_token;
                        unsigned flags = get_flags(current_section);

                        if (was_org) {
                            offset = was_org;
                            was_org = 0;
                        } else
                            offset = 0;


                        add_section("SEG", current_token, offset, 0, flags);


                        state_machine = 2;
                        break;
                    }
                    case DIRECTIVE: {

                        if (current_token == ".global") { // Worry about in second pass
                            while (!token_line.empty())
                                token_line.pop();

                            break;
                        }

                        int last_inc = 0, last_val = 0;

                        while(!token_line.empty()) {
                            string new_token = token_line.front();
                            token_line.pop();

                            if(new_token == "DUP") { // in second pass
                                offset -= last_inc;
                                while(!token_line.empty())
                                    token_line.pop();

                                offset += last_inc * last_val;
                                break;
                            }

                            int value = parse_expression(new_token), bytes = 0;

                            if (current_token == "DB")
                                bytes = 1;
                            else if (current_token == "DW")
                                bytes = 2;
                            else if (current_token == "DD")
                                bytes = 4;
                            else
                                bytes += 4;

                            offset += bytes;
                            last_inc = bytes;
                            last_val = value;

                            if (current_token == "ORG") {
                                was_org = (unsigned) value;
                                offset -= bytes;
                            }
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
                        if (opcode.using_both)
                            offset += 8;
                        else offset += 4;

                        while (!token_line.empty())
                            token_line.pop();

                        break;
                    }

                    case SYMBOL: { // This should only handle DEF directive
                        string def_token = token_line.front(); token_line.pop();
                        if(def_token == "DEF" || def_token == "def") {
                            int value = parse_expression(token_line.front()); token_line.pop();

                            Symbol_Table.push_back(SymTableEntry("SYM", current_token, -1, value, GLOBAL));
                        } else {
                            cerr << "You cannot do that!" << endl;
                            exit(-1);
                        }
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
    //for (auto &i : Symbol_Table)
        //cout << i << endl;
}

bool second_pass_check = false;

void second_pass() {

    offset = 0;
    current_section = "none";
    second_pass_check = true;
    unsigned was_org = 0;

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
                        int ord = find_section_ord(current_section);

                        Sectionlist.push_back(Section(current_section, Symbol_Table[ord - 1].start_adr, was_org != 0, Symbol_Table[ord - 1].size));

                        if (was_org) {
                            offset = was_org;
                            was_org = 0;
                        } else
                            offset = 0;

                        state_machine = 2;
                        break;
                    }

                    case DIRECTIVE: {
                        if (current_token == ".global") {
                            state_machine = 3;
                            break;
                        }

                        int last_inc = 0, last_val = 0;

                        while(!token_line.empty()) {
                            string new_token = token_line.front();
                            token_line.pop();

                            if(new_token == "DUP") {
                                offset -= last_inc;
                                Sectionlist[Sectionlist.size() - 1].decrease(last_inc);

                                new_token = token_line.front();
                                token_line.pop();

                                if(new_token != "?" && current_section.substr(0, 4) == ".bss") {
                                    cerr << "Can't initialize in .bss section" << endl;
                                    exit(-1);
                                } else if (new_token == "?") {
                                    offset += last_inc * last_val;
                                } else if(new_token != "?") {
                                    int value = parse_expression(new_token);
                                    // write to memory
                                    for(int j = 0; j < last_val; j++, offset += last_inc)
                                        Sectionlist[Sectionlist.size() - 1].write(value, last_inc);
                                }
                                break;
                            }

                            int value = parse_expression(new_token), bytes = 0;

                            if (current_token == "DB")
                                bytes = 1;
                            else if (current_token == "DW")
                                bytes = 2;
                            else if (current_token == "DD")
                                bytes = 4;
                            else
                                bytes += 4;

                            offset += bytes;
                            last_inc = bytes;
                            last_val = value;



                            if (current_token == "ORG") {
                                was_org = (unsigned) value;
                                offset -= bytes;
                            } else
                                Sectionlist[Sectionlist.size() - 1].write(value, last_inc);
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
                        //cerr << opcode.get_first_word() << endl;
                        Sectionlist[Sectionlist.size() - 1].write(opcode.get_first_word(), 4);

                        if(opcode.using_both)
                            Sectionlist[Sectionlist.size() - 1].write(opcode.second_word, 4);

                        if (opcode.using_both)
                            offset += 8;
                        else offset += 4;

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
                    state_machine = 2;
                }
            }
        }
    }

    second_pass_check = false;
}

void write_obj(ofstream& out) {
    out << "#TabelaSimbola" << endl;

    for(auto& i : Symbol_Table)
        out << i << endl;

    int last_ord = -1;
    for(int i = 0; i < Relocations.size(); i++) {
        Relocation tmp = Relocations[i];
        if(tmp.ordinal_section_no != last_ord) {
            out << "#rel" << Symbol_Table[tmp.ordinal_section_no - 1].name << endl;
            last_ord = tmp.ordinal_section_no;
        }

        out << tmp << endl;
    }

    for(auto& i:Sectionlist) {
        out << i.name << endl;
        out << i << endl;
    }

    out << "#end";
}