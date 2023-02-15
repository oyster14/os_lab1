#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

struct Token {
    int line = 0;
    int offset = 0;
    string symbol = "";
    Token() : line(), offset(), symbol() {}
    Token(int line, int offset, string symbol)
        : line(line), offset(offset), symbol(symbol) {}
};

struct MemInfo {
    int val = 0;
    int errcode = -1;
    string sym = "";
    MemInfo() : val(), errcode(), sym() {}
    MemInfo(int val, int errcode, string sym)
        : val(val), errcode(errcode), sym(sym) {}
};

void __parse_error(int errcode, int linenum, int lineoffset) {
    static const vector<string> errstr = {
        "NUM_EXPECTED",   // Number expect, anything >= 2^30 is not a number
        "SYM_EXPECTED",   // Symbol Expected
        "ADDR_EXPECTED",  // Addressing Expected which is A/E/I/R
        "SYM_TOO_LONG",   // Symbol Name is too long
        "TOO_MANY_DEF_IN_MODULE",  // > 16
        "TOO_MANY_USE_IN_MODULE",  // > 16
        "TOO_MANY_INSTR",          // total num_instr exceeds memory size (512)
    };
    cout << "Parse Error line " << linenum << " offset " << lineoffset << ": "
         << errstr[errcode] << endl;
    exit(0);
};

void __error_message(int errcode, string symName = "") {
    static const vector<string> errstr = {
        "Absolute address exceeds machine size; zero used",
        "Relative address exceeds module size; zero used",
        "External address exceeds length of uselist; treated as "
        "immediate",
        " is not defined; zero used",
        "This variable is multiple times defined; first value used",
        "Illegal immediate value; treated as 9999",
        "Illegal opcode; treated as 9999"};
    cout << " Error: " << symName << errstr[errcode];
};

void __warning_message(int errcode, string sym = "", int module = 0,
                       int val = 0, int maxVal = 0) {
    static const vector<string> errstr = {
        ") assume zero relative",  // "too big %d (max=%d) assume zero relative"
        " redefined and ignored",
        " appeared in the uselist but was not actually used",
        " was defined but never used"};
    cout << "Warning: Module " << module << ": " << sym;
    if (errcode == 0) {
        cout << " too big " << val << " (max=" << maxVal;
    }
    cout << errstr[errcode] << endl;
};

class SymTab {
   public:
    struct SymInfo {
        int val = 0;
        int loc = 0;
        int warncode = 3;
        int errcode = -1;
        SymInfo() : val(), loc(), warncode(), errcode() {}
        SymInfo(int val, int loc) : val(val), loc(loc) {}
    };

    void createSymbol(string sym, int val, int loc) {
        syms.emplace_back(sym);
        symTable[sym] = {val, loc};
    }

    void print() {
        cout << "Symbol Table" << endl;
        for (string& sym : syms) {
            cout << sym << "=" << symTable[sym].val;
            if (symTable[sym].errcode >= 0) {
                __error_message(symTable[sym].errcode);
            }
            cout << endl;
        }
        cout << endl;
    }

    void printWarning() {
        cout << endl;
        for (string& sym : syms) {
            if (symTable[sym].warncode == 3) {
                __warning_message(3, sym, symTable[sym].loc);
            }
        }
        cout << endl;
    }

    void setMulDefErr(string sym, int module_counter) {
        symTable[sym].errcode = 4;
        __warning_message(1, sym, module_counter);
    }

    bool symExist(string sym) { return symTable.count(sym); }

    void setUsedWarn(string sym) { symTable[sym].warncode = -1; }

    int getVal(string sym) { return symTable[sym].val; }

   private:
    vector<string> syms;
    unordered_map<string, SymInfo> symTable;
};

class MemMap {
   public:
    void allocateMem(int val, int errcode, string sym) {
        memMap.emplace_back(val, errcode, sym);
    }

    void print() {
        int i = memMap.size() - 1;
        if (i == 0) {
            cout << "Memory Map" << endl;
        }
        cout << setw(3) << setfill('0') << i;
        cout << ": " << setw(4) << setfill('0') << memMap[i].val;
        if (memMap[i].errcode >= 0) {
            __error_message(memMap[i].errcode, memMap[i].sym);
        }
        cout << endl;
    }

   private:
    vector<MemInfo> memMap;
};

class Tokenizer {
   public:
    string fileName;

    Tokenizer(string fileName) : fileName{fileName} {
        file.open(fileName);
        if (!file.is_open()) {
            cout << "Unable to open input file." << endl;
            exit(0);
        }
        toNextToken();
    }

    ~Tokenizer() { file.close(); }

    void toNextToken() {
        while (true) {
            while (line == "" || currPos == line.size()) {
                if (!getline(file, line)) {
                    finalLine = lineCounter;
                    finalOffset = currPos + 1;
                    break;
                }
                currPos = 0;
                startPos = 0;
                lineCounter++;
            };
            if (file.eof()) {
                break;
            }
            while (currPos < line.size() &&
                   (line[currPos] == ' ' || line[currPos] == '\t')) {
                currPos++;
            }
            if (currPos != line.size()) {
                break;
            }
        }
    }

    Token getToken() {
        startPos = currPos;
        while (currPos < line.size() &&
               (line[currPos] != ' ' && line[currPos] != '\t')) {
            currPos++;
        }
        Token token = Token(lineCounter, startPos + 1,
                            line.substr(startPos, currPos - startPos));
        toNextToken();
        return token;
    }

    int readInt(bool check = true, int list_type = 0, int mem_used = 0) {
        if (check && isEnd()) {
            __parse_error(0, finalLine, finalOffset);
        }
        Token token = getToken();
        if (check) {
            int idx = 0, len = token.symbol.size();
            while (idx < len - 1 && token.symbol[idx] == '0') {
                idx++;
            }
            if (len - idx > 10 ||
                (len - idx == 10 && token.symbol[idx] != '1')) {
                __parse_error(0, token.line, token.offset);
            }
            while (idx < len) {
                if (!isdigit(token.symbol[idx])) {
                    __parse_error(0, token.line, token.offset);
                }
                idx++;
            }
        }
        int val = stoi(token.symbol);
        if (check && val >= 1073741824) {
            __parse_error(0, token.line, token.offset);
        }
        if (check && list_type > 0) {
            if (list_type < 6) {
                if (val > 16) {
                    __parse_error(list_type, token.line, token.offset);
                }
            } else {
                if (val + mem_used > 512) {
                    __parse_error(list_type, token.line, token.offset);
                }
            }
        }
        return val;
    }

    MemInfo readInstr(char type, int module_base, int module_size,
                      vector<pair<string, bool>>& uselist, SymTab* symTab,
                      MemMap* memMap) {
        int instr = readInt(false);
        int opcode = instr / 1000;
        int operand = instr % 1000;
        int errcode = -1;  // instr, errcode
        string sym = "";
        if (opcode >= 10) {
            if (type == 'I') {
                errcode = 5;
            } else {
                errcode = 6;
            }
            return {9999, errcode, sym};
        }
        switch (type) {
            case 'R':
                if (operand >= module_size) {
                    instr = opcode * 1000 + module_base;
                    errcode = 1;
                } else {
                    instr += module_base;
                }
                break;
            case 'E':
                if (operand >= uselist.size()) {
                    errcode = 2;
                } else {
                    instr = opcode * 1000;
                    sym = uselist[operand].first;
                    uselist[operand].second = true;
                    if (symTab->symExist(sym)) {
                        symTab->setUsedWarn(sym);
                        instr += symTab->getVal(sym);
                    } else {
                        errcode = 3;
                    }
                }
                break;
            case 'A':
                if (operand >= 512) {
                    instr = opcode * 1000;
                    errcode = 0;
                }
            case 'I':
                break;
            default:
                cout << "wrong type" << endl;
                exit(0);
        }
        return {instr, errcode, sym};
    }

    string readSym(bool check = true) {
        if (check && isEnd()) {
            __parse_error(1, finalLine, finalOffset);
        }
        Token token = getToken();
        if (check) {
            int len = token.symbol.size();
            if (!isalpha(token.symbol[0])) {
                __parse_error(1, token.line, token.offset);
            }
            for (int i = 1; i < len; i++) {
                if (!isalnum(token.symbol[i])) {
                    __parse_error(1, token.line, token.offset);
                }
            }
            if (len > 16) {
                __parse_error(3, token.line, token.offset);
            }
        }
        return token.symbol;
    }

    char readIAER(bool check = true) {
        if (check && isEnd()) {
            __parse_error(2, finalLine, finalOffset);
        }
        Token token = getToken();
        char type = token.symbol[0];
        if (check) {
            if (token.symbol.size() != 1 ||
                !(token.symbol == "I" || token.symbol == "A" ||
                  token.symbol == "E" || token.symbol == "R")) {
                __parse_error(2, token.line, token.offset);
            }
        }
        return type;
    }

    bool isEnd() { return file.eof(); }

    void backToBeginning() {
        if (!isEnd()) {
            cout << "Current file has yet to reach the end!" << endl;
            exit(0);
        }
        line = "";
        startPos = 0;
        currPos = 0;
        lineCounter = 0;
        finalLine = 0;
        finalOffset = 0;
        file.clear();
        file.seekg(0);
        toNextToken();
    }

    void printTokens() {
        while (!isEnd()) {
            Token token = getToken();
            cout << "Token: " << token.line << ":" << token.offset << " : "
                 << token.symbol << endl;
        }
        cout << "Final Spot in File : line=" << finalLine
             << " offset=" << finalOffset << endl;
    }

   private:
    ifstream file;
    string line = "";
    int startPos = 0;
    int currPos = 0;
    int lineCounter = 0;
    int finalLine = 0;
    int finalOffset = 0;
};

void Pass1(Tokenizer* tokenizer, SymTab* symTab) {
    int module_base = 0;
    int module_counter = 0;
    int mem_used = 0;
    vector<pair<string, int>> deflist;

    while (!tokenizer->isEnd()) {
        module_counter++;

        int defcount = tokenizer->readInt(true, 4);
        deflist.clear();
        for (int i = 0; i < defcount; i++) {
            string sym = tokenizer->readSym();
            int val = tokenizer->readInt();
            deflist.emplace_back(sym, val);
        }

        int usecount = tokenizer->readInt(true, 5);
        for (int i = 0; i < usecount; i++) {
            string sym = tokenizer->readSym();
        }

        int instcount = tokenizer->readInt(true, 6, mem_used);
        mem_used += instcount;
        for (int i = 0; i < instcount; i++) {
            char addressmode = tokenizer->readIAER();
            int operand = tokenizer->readInt();
        }

        for (auto& sym_val : deflist) {
            int val = sym_val.second;
            string& sym = sym_val.first;
            if (symTab->symExist(sym) == false) {
                if (val < instcount) {
                    val += module_base;
                } else {
                    __warning_message(0, sym, module_counter, val,
                                      instcount - 1);
                    val = module_base;
                }
                symTab->createSymbol(sym, val, module_counter);
            } else {
                symTab->setMulDefErr(sym, module_counter);
            }
        }
        module_base += instcount;
    }

    symTab->print();
}

void Pass2(Tokenizer* tokenizer, SymTab* symTab, MemMap* memMap) {
    int module_base = 0;
    int module_counter = 0;
    vector<pair<string, bool>> uselist;

    while (!tokenizer->isEnd()) {
        module_counter++;

        int defcount = tokenizer->readInt(false);
        for (int i = 0; i < defcount; i++) {
            string sym = tokenizer->readSym(false);
            int val = tokenizer->readInt(false);
        }

        int usecount = tokenizer->readInt(false);
        uselist.clear();
        for (int i = 0; i < usecount; i++) {
            string sym = tokenizer->readSym(false);
            uselist.emplace_back(sym, false);
        }

        int instcount = tokenizer->readInt();

        for (int i = 0; i < instcount; i++) {
            char addrmode = tokenizer->readIAER(false);
            MemInfo memInfo = tokenizer->readInstr(
                addrmode, module_base, instcount, uselist, symTab, memMap);
            memMap->allocateMem(memInfo.val, memInfo.errcode, memInfo.sym);
            memMap->print();
        }

        for (auto& use : uselist) {
            if (use.second == false) {
                __warning_message(2, use.first, module_counter);
            }
        }

        module_base += instcount;
    }

    symTab->printWarning();
};

int main(int argc, char* argv[]) {
    string fileName = "";
    if (argc == 2) {
        fileName = argv[1];
    } else {
        cout << "Wrong input!" << endl;
        return -1;
    }

    SymTab* symTab = new SymTab();

    MemMap* memMap = new MemMap();

    Tokenizer* tokenizer = new Tokenizer(fileName);

    Pass1(tokenizer, symTab);

    tokenizer->backToBeginning();

    Pass2(tokenizer, symTab, memMap);

    delete tokenizer;

    delete memMap;

    delete symTab;

    return 0;
}