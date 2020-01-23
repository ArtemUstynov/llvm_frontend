//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//
#ifndef COMP_LEXAN_H
#define COMP_LEXAN_H

#include <cstdio>
#include <cctype>
#include <string>

//#include "lexan.h"

using namespace std;

extern string IdentifierStr; // Filled in if tok_identifier
extern int NumVal;             // Filled in if tok_number

bool isexpr(char c);

typedef enum Token {
    tok_eof = -1,

    tok_prog = -2,
    tok_func = -3,
    tok_beg = -4,
    tok_end = -7,
    tok_identifier = -8,
    tok_number = -9,
    tok_int = -10,
    tok_extern = -11,
    tok_if = -12,
    tok_then = -13,
    tok_else = -14,
    tok_for = -15,
    tok_to = -16,
    tok_do = -17,
    tok_ass = -18,
    tok_var = -19,
    tok_const = -20,
    tok_downto = -21,
    tok_ge = -22,
    tok_le = -23,
    tok_eq = -24,
    tok_ne = -25,
//    tok_ret = -25,
    tok_procedure = -26,
} Token;


/// gettok - Return the next token from standard input.
int gettok();

#endif