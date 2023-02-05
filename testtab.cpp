#include <iostream>
using namespace std;

char ops[] = {'\t', ' '};

int main() {
    srand(time(0));
    int line = 10 + rand() % 10;
    int offset = 15 + rand() % 12;
    for (int i = 0; i < line; i++) {
        for (int j = 0; j < offset; j++) {
            int op = rand() % 2;
            cout << "a" << ops[op];
        }
        cout << endl;
    }
    return 0;
}