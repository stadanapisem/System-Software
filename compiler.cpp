#include <string>
#include <vector>
#include <cstring>
#include <regex>
#include <iostream>
#include <queue>
#include "compiler.h"

using namespace std;

vector<vector<string> > Tokens;
vector<SymTableEntry> Symbol_Table;
vector<Section> Sections;

vector<pair<string, unsigned>> Instructions = {
        pair<string, unsigned>("int", 0x00),
        pair<string, unsigned>("jmp", 0x02),
        pair<string, unsigned>("call", 0x03),
        pair<string, unsigned>("ret", 0x01),
        pair<string, unsigned>("jz", 0x04),
        pair<string, unsigned>("jnz", 0x05),
        pair<string, unsigned>("jgz", 0x06),
        pair<string, unsigned>("jgez", 0x07),
        pair<string, unsigned>("jlz", 0x08),
        pair<string, unsigned>("jlez", 0x09),

        pair<string, unsigned>("load", 0x10),
        pair<string, unsigned>("store", 0x11),

        pair<string, unsigned>("push", 0x20),
        pair<string, unsigned>("pop", 0x21),

        pair<string, unsigned>("add", 0x30),
        pair<string, unsigned>("sub", 0x31),
        pair<string, unsigned>("mul", 0x32),
        pair<string, unsigned>("div", 0x33),
        pair<string, unsigned>("mod", 0x34),
        pair<string, unsigned>("and", 0x35),
        pair<string, unsigned>("or", 0x36),
        pair<string, unsigned>("xor", 0x37),
        pair<string, unsigned>("not", 0x38),
        pair<string, unsigned>("asl", 0x39),
        pair<string, unsigned>("asr", 0x3A)
};

vector<Match> Matcher = {
        {.type = LABEL, .pattern = regex("^([a-zA-Z0-9]{1,}):$")},
        {.type = SYMBOL, .pattern = regex("^([a-zA-Z_])([a-zA-Z0-9])*$")},
        {.type = DIRECTIVE, .pattern = regex("^(def|DUP|ORG|DD|DW|DB|.global)$")},
        {.type = SECTION, .pattern = regex("^.(text|data|rodata|bss)(.[0-9]+)?$")},
        {.type = INSTRUCTION, .pattern = regex(
                "^(int|jmp|call|ret|jz|jnz|jgz|jgez|jlz|jlez|load|store|push|pop|add|sub|mul|div|mod|and|or|xor|not|asl|asr)$")},
        {.type = OPR_DEC, .pattern = regex("^[0-9]+$")},
        {.type = OPR_HEX, .pattern = regex("^0x[0-9a-fA-F]+$")}
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

static token_t find_match(string token) {
    token_t ret = ILLEGAL;

    if (regex_match(token, Matcher[SYMBOL].pattern))
        ret = SYMBOL;

    //std::transform(token.begin(), token.end(), token.begin(), ::tolower);
    string lower = "";
    lower.reserve(token.size());
    for (int i = 0; i < token.size(); i++)
        lower += tolower(token[i]);

    for (auto &i : Matcher) {
        if (i.type == SYMBOL)
            continue;

        if (regex_match(token, i.pattern) || regex_match(lower, i.pattern)) {
            if (ret == SYMBOL || ret == ILLEGAL)
                ret = i.type;
            else
                cerr << "ERROR" << endl;
        }

    }

    return ret;
}

static bool find_symbol(string name) {
    for (auto &i : Symbol_Table)
        if (i.name == name)
            return true;

    return false;
}

static void add_symbol(string name, string section, unsigned offset, scope_t scope, int size) {
    SymTableEntry sym = {.name = name, .section = section,
            .offset = offset, .scope = scope, .size = size};

    if (find_symbol(name)) {
        cerr << "Symbol already in table" << endl;
        exit(-1);
    }

    Symbol_Table.push_back(sym);

}

static SectionName toSectionName(string name) {
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

static unsigned getOperandValue(string token) {
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

    /*for (auto &i : Tokens) {
        for (auto &j : i)
            fprintf(stderr, "%s\n", j.c_str());
        fprintf(stderr, "-------------\n");
    }*/

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
                    add_symbol(current_token.substr(0, current_token.size() - 1), current_section, offset, LOCAL, 0);
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
                            Sections.push_back(Section(current_section, offset));

                            for (auto &sym : Symbol_Table)
                                if (sym.name == current_token)
                                    sym.size = offset - sym.offset;
                        }

                        current_section = current_token;
                        add_symbol(current_token, current_section, offset, LOCAL, 0);

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
                        token_t new_token_type = find_match(new_token);

                        if (new_token_type != OPR_DEC && new_token_type != OPR_HEX && new_token_type != SYMBOL) {
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
                        offset += 4; // TODO Properly determine instruction size

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

    Sections.push_back(Section(current_section, offset));
    for (auto &sym : Symbol_Table)
        if (sym.name == current_section)
            sym.size = offset - sym.offset;

    for (auto &i : Sections)
        cout << i.name << " " << i.size << endl;
    cout << "------------------------" << endl;
    for (auto &i : Symbol_Table)
        cout << i.name << " " << i.section << " " << i.size << " " << i.offset << endl;
}

void second_run() {

}