//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

#include <cstdio>
#include <cctype>
#include <string>
#include <iostream>
#include <cstdlib>
#include "lexan.h"

using namespace std;
string IdentifierStr; // Filled in if tok_identifier
int NumVal;             // Filled in if tok_number
/// gettok - Return the next token from standard input.
bool isexpr(char c) {
    return ((c > 57 && c < 63)) && c != 59;
}

int gettok() {
    static int LastChar = ' ';
    // Skip any whitespace.
    while (isspace(LastChar)) {
        LastChar = getchar();
    }
    if (isalpha(LastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar())))
            IdentifierStr += LastChar;
        if (IdentifierStr == "function")
            return tok_func;
        if (IdentifierStr == "program")
            return tok_prog;
        if (IdentifierStr == "begin")
            return tok_beg;
        if (IdentifierStr == "end")
            return tok_end;
        if (IdentifierStr == "integer")
            return tok_int;
        if (IdentifierStr == "extern")
            return tok_extern;
        if (IdentifierStr == "if")
            return tok_if;
        if (IdentifierStr == "then")
            return tok_then;
        if (IdentifierStr == "else")
            return tok_else;
        if (IdentifierStr == "for")
            return tok_for;
        if (IdentifierStr == "to")
            return tok_to;
        if (IdentifierStr == "do")
            return tok_do;
        if (IdentifierStr == "var")
            return tok_var;
        if (IdentifierStr == "const")
            return tok_const;
        if (IdentifierStr == "downto")
            return tok_downto;
        if (IdentifierStr == "procedure")
            return tok_procedure;
        return tok_identifier;
    }

    if (isdigit(LastChar)) { // Number: [0-9.]+
        string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar));

        NumVal = (int) strtod(NumStr.c_str(), nullptr);
        return tok_number;
    }

    if (LastChar == '&') { // Number: [0-9.]+
        LastChar = getchar();
        string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar));
        char * pend;
        NumVal = (int) strtol(NumStr.c_str(), &pend, 8);

        return tok_number;
    }
    if (LastChar == '$') { // Number: [0-9.]+
        LastChar = getchar();
        string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar)||isalnum(LastChar));
        char * pend;
        NumVal = (int) strtol(NumStr.c_str(), &pend, 16);

        return tok_number;
    }

    if (LastChar == '#') {
        // Comment until end of line.
        do
            LastChar = getchar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
    }
    if (isexpr(LastChar)) {
        string exprStr;
        exprStr = LastChar;
        while (isexpr(cin.peek())) {
            LastChar = getchar();
            exprStr += LastChar;
            if (exprStr == ":=") {
                LastChar = getchar();
                return tok_ass;
            }
            if (exprStr == ">=") {
                LastChar = getchar();
                return tok_ge;
            }
            if (exprStr == "<=") {
                LastChar = getchar();
                return tok_le;
            }
            if (exprStr == "==") {
                LastChar = getchar();
                return tok_eq;
            }
            if (exprStr == "!=") {
                LastChar = getchar();
                return tok_ne;
            }
        }
    }

    // Check for end of file.  Don't eat the EOF.
    if (LastChar == EOF)
        return tok_eof;

    // Otherwise, just return the character as its ascii value.
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}
