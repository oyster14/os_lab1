#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

class Tokenizer {
   public:
    string fileName;

    Tokenizer(string fileName) : fileName{fileName} {
        file.open(fileName);
        if (!file.is_open()) {
            cout << "Unable to open input file." << endl;
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
            while (currPos < line.size() && line[currPos] == ' ') {
                currPos++;
            }
            if (currPos != line.size()) {
                break;
            }
        }
    }

    Token getToken() {
        startPos = currPos;
        while (currPos < line.size() && line[currPos] != ' ') {
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
                return -1;
            }
        }
        return stoi(token.symbol);
    }

    int readInstr(char type, int module_base, vector<string>& symsOfModule,
                  SymTab* symTab) {
        int instr = readInt();
        int opcode = instr / 1000;
        int operand = instr % 1000;
        if (opcode >= 10) {
            cout << "wrong opcode" << endl;
        }
        switch (type) {
            case 'R':
                instr += module_base;
                break;
            case 'E':
                if (operand >= symsOfModule.size()) {
                    cout << "wrong operand" << endl;
                }
                instr = opcode * 1000 + symTab->getVal(symsOfModule[operand]);
                break;
            case 'A':
                if (operand >= 512) {
                    cout << "larger than machine size" << endl;
                    // instr changed
                }
            case 'I':
                break;
            default:
                cout << "wrong type" << endl;
        }
        return instr;
    }

    string readSym() {
        Token token = getToken();
        for (int i = 0; i < token.symbol.size(); i++) {
            if (!isalnum(token.symbol[i])) {
                cout << "wrong symbol token" << endl;
            }
        }
        return token.symbol;
    }

    char readIAER() {
        Token token = getToken();
        if (token.symbol.size() != 1 || token.symbol != "I" ||
            token.symbol != "A" || token.symbol != "E" || token.symbol != "R") {
            cout << "wrong IAER token" << endl;
        }
        return token.symbol[0];
    }

    bool isEnd() { return file.eof(); }

    void backToBeginning() {
        if (!isEnd()) {
            cout << "file has yet to reach the end!" << endl;
            return;
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

class SymTab {
   public:
    void createSymbol(string sym, int val) { symTab[sym] = val; }

    void print() {
        // cout << "Symbol Table" << endl;
        // for (auto [sym, val] : symTab) {
        //     cout << sym << "=" << val << endl;
        // }
        // cout << endl;
    }

    int getVal(string sym) {
        if (symTab.count(sym) == 0) {
            cout << "wrong sym" << endl;
        }
        return symTab[sym];
    }

   private:
    unordered_map<string, int> symTab;
    unordered_set<string> undefSym;
};

void Pass1(Tokenizer* tokenizer, SymTab* symTab) {
    int module_base = 0;

    while (!tokenizer->isEnd()) {
        int defcount = tokenizer->readInt();
        for (int i = 0; i < defcount; i++) {
            string sym = tokenizer->readSym();
            int val = tokenizer->readInt();
            symTab->createSymbol(sym, val);
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
    }

    symTab->print();
}

void Pass2(Tokenizer* tokenizer, SymTab* SymTab) {}

int main(int argc, char* argv[]) {
    string fileName = "";
    if (argc == 2) {
        fileName = argv[1];
    } else {
        cout << "Wrong input!" << endl;
        return -1;
    }

    Tokenizer* tokenizer = new Tokenizer(fileName);

    SymTab* symTab = new SymTab();

    tokenizer->printTokens();

    Pass1(tokenizer, symTab);

    Pass2(tokenizer, symTab);

    delete symTab;

    delete tokenizer;

    return 0;
}
