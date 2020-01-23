#ifndef COMP_PARSER_H
#define COMP_PARSER_H
//
// Created by Ustynov, Artem on 2019-05-03.
//

/* parser.c */
#include <cstdio>
#include "llvm/ADT/STLExtras.h"
#include "lexan.h"
#include "ExprAST.h"
#include <map>
#include <iostream>

#define DLLEXPORT

using namespace std;
/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.
extern int CurTok;


/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
extern map<char, int> BinopPrecedence;

unique_ptr<ExprAST> ParseExpression();

///getNextToken reads another token from the lexer and updates CurTok with its results.
int getNextToken();

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int GetTokPrecedence();

/// LogError* - These are little helper functions for error handling.
unique_ptr<ExprAST> LogError(const char * Str);

unique_ptr<PrototypeAST> LogErrorP(const char * Str);

void swallow_int(bool mand);

unique_ptr<ExprAST> ParseExpression();

/// numberexpr ::= number
unique_ptr<ExprAST> ParseNumberExpr();

/// parenexpr ::= '(' expression ')'
unique_ptr<ExprAST> ParseParenExpr();

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
unique_ptr<ExprAST> ParseIdentifierExpr();

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
unique_ptr<ExprAST> ParsePrimary();

/// binoprhs
///   ::= ('+' primary)*
unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS);

/// expression
///   ::= primary binoprhs
///
unique_ptr<ExprAST> ParseExpression();

/// prototype
///   ::= id '(' id* ')'
unique_ptr<PrototypeAST> ParsePrototype();


/// definition ::= 'def' prototype expression
unique_ptr<FunctionAST> ParseDefinition();

/// toplevelexpr ::= expression
unique_ptr<FunctionAST> ParseTopLevelExpr();

/// external ::= 'extern' prototype
unique_ptr<PrototypeAST> ParseExtern();

unique_ptr<ExprAST> ParseIfExpr();

/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
unique_ptr<ExprAST> ParseForExpr();

unique_ptr<ExprAST> ParseUnary();


static std::unique_ptr<ExprAST> ParseVarExpr();
//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

void HandleDefinition();

void HandleExtern();

void HandleTopLevelExpression();

void HandleGlobalVar();

void HandleConst();

/// top ::= definition | external | expression | ';'
void MainLoop();

extern "C" DLLEXPORT double putchard(double X);

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X);

#endif