//
// Created by DevStation on 20.02.2022.
//

#include "read_input_functions.h"

using namespace  std;
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}
int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}
