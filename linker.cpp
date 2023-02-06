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

void __warning_message(int errcode, string sym = "", int module = 0) {
    static const vector<string> errstr = {
        " too big %d (max=%d) assume zero relative",  // for format
        " redefined and ignored",
        " appeared in the uselist but was not actually used",
        " was defined but never used"};
    cout << "Warning: Module " << module << ": " << sym << errstr[errcode]
         << endl;
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
        if (symTable.count(sym) == 0) {
            syms.emplace_back(sym);
            symTable[sym] = {val, loc};
        } else {
            __warning_message(1, sym, loc);
            symTable[sym].errcode = 4;
        }
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

    void setUsedWarn(string sym) { symTable[sym].warncode = -1; }

    int getVal(string sym) { return symTable[sym].val; }

    bool symExist(string sym) { return symTable.count(sym); }

   private:
    vector<string> syms;
    unordered_map<string, SymInfo> symTable;
};

class MemMap {
   public:
    void allocateMem(int val, int errcode, string sym) {
        if (memMap.size() == 256) {
            cout << "exceed maximum memory" << endl;
            exit(0);
        }
        memMap.emplace_back(val, errcode, sym);
    }

    void print() {
        int i = memMap.size() - 1;
        if (i == 0) {
            cout << "Memory Map" << endl;
        }
        cout << setw(3) << setfill('0') << i;
        cout << ": " << memMap[i].val;
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

    int readInt(bool check = true) {
        Token token = getToken();
        if (check) {
            for (int i = 0; i < token.symbol.size(); i++) {
                if (!isdigit(token.symbol[i])) {
                    cout << "wrong int token" << endl;
                    exit(0);
                }
            }
        }
        return stoi(token.symbol);
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
            cout << "wrong opcode" << endl;
            exit(0);
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
        Token token = getToken();
        if (check) {
            int len = token.symbol.size();
            if (len > 16) {
                cout << "wrong symbol token" << endl;
                exit(0);
            }
            for (int i = 0; i < len; i++) {
                if (!isalnum(token.symbol[i])) {
                    cout << "wrong symbol token" << endl;
                    exit(0);
                }
            }
        }
        return token.symbol;
    }

    char readIAER(bool check = true) {
        Token token = getToken();
        char type = token.symbol[0];
        if (check) {
            if (token.symbol.size() != 1 ||
                !(token.symbol == "I" || token.symbol == "A" ||
                  token.symbol == "E" || token.symbol == "R")) {
                cout << "syntax error: an unexpected token" << endl;
                exit(0);
            }
        }
        return type;
    }

    bool isEnd() { return file.eof(); }

    void backToBeginning() {
        if (!isEnd()) {
            cout << "file has yet to reach the end!" << endl;
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

    while (!tokenizer->isEnd()) {
        module_counter++;

        int defcount = tokenizer->readInt();
        if (defcount > 16) {
            cout << "defcount exceeds maximum limit" << endl;
            exit(0);
        }
        for (int i = 0; i < defcount; i++) {
            string sym = tokenizer->readSym();
            int val = tokenizer->readInt();
            symTab->createSymbol(sym, val + module_base, module_counter);
        }

        int usecount = tokenizer->readInt();
        for (int i = 0; i < usecount; i++) {
            string sym = tokenizer->readSym();
        }

        int instcount = tokenizer->readInt();
        for (int i = 0; i < instcount; i++) {
            char addressmode = tokenizer->readIAER();
            int operand = tokenizer->readInt();
        }
        module_base += instcount;
    }

    symTab->print();
}

void Pass2(Tokenizer* tokenizer, SymTab* symTab, MemMap* memMap) {
    int module_base = 0;
    int module_counter = 0;
    vector<pair<string, bool>> uselist;
    vector<string> deflist;

    while (!tokenizer->isEnd()) {
        module_counter++;

        int defcount = tokenizer->readInt(false);
        deflist.clear();
        for (int i = 0; i < defcount; i++) {
            string sym = tokenizer->readSym(false);
            int val = tokenizer->readInt(false);
            deflist.emplace_back(sym);
        }

        int usecount = tokenizer->readInt(false);
        if (usecount > 16) {
            cout << "usecount exceeds maximum limit" << endl;
            exit(0);
        }
        uselist.clear();
        for (int i = 0; i < usecount; i++) {
            string sym = tokenizer->readSym(false);
            uselist.emplace_back(sym, false);
            if (symTab->symExist(sym)) {
                symTab->setUsedWarn(sym);
            }
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

    delete symTab;

    delete memMap;

    delete tokenizer;

    return 0;
}