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

void __warning_message(int errcode, string symName = "", int symVal = 0) {
    static const vector<string> errstr = {
        " too big %d (max=%d) assume zero relative",  // see input - 4
        " redefined and ignored",
        " appeared in the uselist but was not actually used",
        " was defined but never used"};
    cout << "Warning: Module " << symVal << ": " << symName << errstr[errcode]
         << endl;
};

class SymTab {
   public:
    void createSymbol(string sym, int val) {
        if (symTab.count(sym) > 0) {
            cout << "duplicate definition" << endl;
            exit(0);
        }
        syms.emplace_back(sym);
        symTab[sym] = make_pair(val, -1);
    }

    void print() {
        cout << "Symbol Table" << endl;
        for (string& sym : syms) {
            cout << sym << "=" << symTab[sym].first;
            if (symTab[sym].second >= 0) {
                __error_message(symTab[sym].second);
            }
            cout << endl;
        }
        cout << endl;
    }

    int getVal(string sym) {
        if (symTab.count(sym) == 0) {
            cout << "wrong sym" << endl;
            exit(0);
        }
        return symTab[sym].first;
    }

   private:
    vector<string> syms;
    unordered_map<string, pair<int, int>> symTab;
};

class MemMap {
   public:
    void allocateMem(int val) {
        if (memMap.size() == 256) {
            cout << "exceed maximum memory" << endl;
            exit(0);
        }
        memMap.emplace_back(val, -1);
    }

    void print() {
        cout << "Memory Map" << endl;
        for (int i = 0; i < memMap.size(); i++) {
            cout << setw(3) << setfill('0') << i;
            cout << ": " << memMap[i].first;
            cout << endl;
        }
    }

   private:
    vector<pair<int, int>> memMap;
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

    int readInt() {
        Token token = getToken();
        for (int i = 0; i < token.symbol.size(); i++) {
            if (!isdigit(token.symbol[i])) {
                cout << "wrong int token" << endl;
                exit(0);
            }
        }
        return stoi(token.symbol);
    }

    int readInstr(char type, int module_base, vector<string>& uselist,
                  SymTab* symTab) {
        int instr = readInt();
        int opcode = instr / 1000;
        int operand = instr % 1000;
        if (opcode >= 10) {
            cout << "wrong opcode" << endl;
            exit(0);
        }
        switch (type) {
            case 'R':
                instr += module_base;
                break;
            case 'E':
                if (operand >= uselist.size()) {
                    cout << "wrong operand" << endl;
                    exit(0);
                }
                instr = opcode * 1000 + symTab->getVal(uselist[operand]);
                break;
            case 'A':
                if (operand >= 512) {
                    cout << "larger than machine size" << endl;
                    exit(0);
                    // instr changed
                }
            case 'I':
                break;
            default:
                cout << "wrong type" << endl;
                exit(0);
        }
        return instr;
    }

    string readSym() {
        Token token = getToken();
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
        return token.symbol;
    }

    char readIAER() {
        Token token = getToken();
        if (token.symbol.size() != 1) {
            cout << "syntax error: an unexpected token" << endl;
            exit(0);
        }
        char type = token.symbol[0];
        if (!(token.symbol == "I" || token.symbol == "A" ||
              token.symbol == "E" || token.symbol == "R")) {
            cout << "syntax error: an unexpected token" << endl;
            exit(0);
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

    while (!tokenizer->isEnd()) {
        int defcount = tokenizer->readInt();
        if (defcount > 16) {
            cout << "defcount exceeds maximum limit" << endl;
            exit(0);
        }
        for (int i = 0; i < defcount; i++) {
            string sym = tokenizer->readSym();
            int val = tokenizer->readInt();
            symTab->createSymbol(sym, val + module_base);
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
    vector<string> uselist;

    while (!tokenizer->isEnd()) {
        int defcount = tokenizer->readInt();
        for (int i = 0; i < defcount; i++) {
            string sym = tokenizer->readSym();
            int val = tokenizer->readInt();
        }

        int usecount = tokenizer->readInt();
        if (usecount > 16) {
            cout << "usecount exceeds maximum limit" << endl;
            exit(0);
        }
        uselist.clear();
        for (int i = 0; i < usecount; i++) {
            string sym = tokenizer->readSym();
            uselist.emplace_back(sym);
        }

        int instcount = tokenizer->readInt();
        for (int i = 0; i < instcount; i++) {
            char addrmode = tokenizer->readIAER();
            int addr =
                tokenizer->readInstr(addrmode, module_base, uselist, symTab);
            memMap->allocateMem(addr);
        }
        module_base += instcount;
    }

    memMap->print();
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