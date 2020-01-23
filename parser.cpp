//
// Created by Ustynov, Artem on 2019-05-03.
//

/* parser.c */
#include <cstdio>
#include "llvm/ADT/STLExtras.h"
#include "ExprAST.h"
#include <map>
#include <iostream>
#include "parser.h"


using namespace std;
/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.
int CurTok;
map<char, int> BinopPrecedence;


unique_ptr<ExprAST> ParseExpression();

int getNextToken() {
    return CurTok = gettok();
}

/*int peekNextToken() {
    return CurTok = gettok();
}*/

int GetTokPrecedence() {
    if (!isascii(CurTok) && CurTok > tok_downto)
        return -1;

    // Make sure it's a declared binop.
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0)
        return -1;
    return TokPrec;
}

unique_ptr<ExprAST> LogError(const char * Str) {
    fprintf(stderr, "CURTOK: %d Error: %s\n", CurTok, Str);
    return nullptr;
}

unique_ptr<PrototypeAST> LogErrorP(const char * Str) {
    LogError(Str);
    return nullptr;
}

void swallow_int(bool mand) {
    if (mand) {
        if (CurTok == ':')
            getNextToken(); // eat :
        else {
            LogErrorP(&"Expected ':' "[CurTok]);
            return;
        }

        if (CurTok == tok_int)
            getNextToken(); // ear integer
        else {
            cout << "--" << IdentifierStr << endl;
            LogErrorP("Expected 'integer' ");
        }
    }
    else {
        if (CurTok == ':')
            swallow_int(true);
    }


}

unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = llvm::make_unique<NumberExprAST>(NumVal);
    // printf("HERE1\n");
    getNextToken(); // consume the number
    //printf("HERE2\n");
    return move(Result);
}

unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken(); // eat (.
    auto V = ParseExpression();
    if (!V)
        return nullptr;

    if (CurTok != ')')
        return LogError("expected ')'");
    getNextToken(); // eat ).
    return V;
}

unique_ptr<ExprAST> ParseIdentifierExpr() {
    string IdName = IdentifierStr;

    getNextToken(); // eat identifier.

    if (CurTok != '(') // Simple variable ref.
        return llvm::make_unique<VariableExprAST>(IdName);

    // Call.
    getNextToken(); // eat (
    vector<unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while (true) {
            if (auto Arg = ParseExpression())
                Args.push_back(move(Arg));
            else
                return nullptr;

            if (CurTok == ')')
                break;

            if (CurTok != ',')
                return LogError("Expected ')' or ',' in argument list");
            getNextToken();
        }
    }

    // Eat the ')'.
    getNextToken();

    if (CurTok == ';')
        getNextToken();
    // Eat the ';'.

    return llvm::make_unique<CallExprAST>(IdName, move(Args));
}

unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        default:
            if (CurTok == tok_beg || CurTok == tok_end) {
                return nullptr;
            }
            cout << "------> ERROR : " << CurTok << endl;
            return LogError("unknown token when expecting an expression");
        case tok_identifier:
            return ParseIdentifierExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        case tok_if:
            return ParseIfExpr();
        case tok_for:
            return ParseForExpr();
        case tok_var:
            return ParseVarExpr();
    }
}

unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, unique_ptr<ExprAST> LHS) {
    // If this is a binop, find its precedence.
    while (true) {
        int TokPrec = GetTokPrecedence();

        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        if (TokPrec < ExprPrec)
            return LHS;

        // Okay, we know this is a binop.
        int BinOp = CurTok;
        getNextToken(); // eat binop

        // Parse the unary expression after the binary operator.
        auto RHS = ParseUnary();
        if (!RHS)
            return nullptr;

        // If BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, move(RHS));
            if (!RHS)
                return nullptr;
        }

        // Merge LHS/RHS.
        LHS = llvm::make_unique<BinaryExprAST>(BinOp, move(LHS), move(RHS));
    }
}

unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParseUnary();
    if (!LHS)
        return nullptr;

    return ParseBinOpRHS(0, move(LHS));
}

unique_ptr<PrototypeAST> ParsePrototype() {
    bool procedure = false;
    if (CurTok == tok_procedure) {
        procedure = true;
        getNextToken(); // eat procedure
    }
    if (CurTok != tok_identifier)
        return LogErrorP("Expected function name in prototype");

    string FnName = IdentifierStr;
    getNextToken();

    if (CurTok != '(')
        return LogErrorP("Expected '(' in prototype");
// function a ( n : integer ) begin 3 end ;
    vector<string> ArgNames;
    while (CurTok != ')' && getNextToken() == tok_identifier) {
        ArgNames.push_back(IdentifierStr);
        getNextToken();
        swallow_int(true);
    }

    if (CurTok != ')')
        return LogErrorP("Expected ')' in prototype");

    getNextToken(); // eat ')'.

    swallow_int(false);
    // success.

    return llvm::make_unique<PrototypeAST>(FnName, move(ArgNames), false, 0, procedure);
}

unique_ptr<FunctionAST> ParseDefinition() {
    bool procedure = false;
    if (CurTok == tok_procedure)
        procedure = true;
    else getNextToken(); // eat def.
    auto Proto = ParsePrototype();
    if (!Proto)
        return nullptr;
    swallow_int(false);
    if (CurTok == ';')
        getNextToken();
    if (CurTok != tok_beg && CurTok != tok_var) {
        LogErrorP("NO BEGIN");
    }
    vector<unique_ptr<ExprAST>> asts;

    if (CurTok == tok_var)
        asts.push_back(ParseExpression());

    if (CurTok == tok_beg) {
        getNextToken(); //eat begin
    }
    while (CurTok != tok_end) {
        if (auto E = ParseExpression()) {
            if (CurTok == ';')
                getNextToken();
            asts.push_back(move(E));
        }
        else {
            if (CurTok == tok_end) {
                return llvm::make_unique<FunctionAST>(move(Proto), move(asts), procedure);
            }
            return nullptr;
        }
    }
    return llvm::make_unique<FunctionAST>(move(Proto), move(asts), procedure);
}

unique_ptr<FunctionAST> ParseTopLevelExpr() {
    vector<unique_ptr<ExprAST>> v;
    if (auto E = ParseExpression()) {
        // Make an anonymous proto.
        v.push_back(move(E));
        auto Proto = llvm::make_unique<PrototypeAST>("main", vector<string>());
        return llvm::make_unique<FunctionAST>(move(Proto), move(v));
    }
    return nullptr;
}

unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat extern.
    return ParsePrototype();
}

/// ifexpr ::= 'if' expression 'then' expression 'else' expression
unique_ptr<ExprAST> ParseIfExpr() {
    getNextToken();  // eat the if.

    // condition.
    auto Cond = ParseExpression();
    if (!Cond)
        return nullptr;

    if (CurTok != tok_then)
        return LogError("expected then");
    getNextToken();  // eat the then

    vector<unique_ptr<ExprAST>> then;

    bool small_if = false;
    if (CurTok != tok_beg)
        small_if = true;
    else getNextToken();  // eat the begin

    while (CurTok != tok_end) {
        if (auto E = ParseExpression()) {
            if (CurTok == ';')
                getNextToken();
            then.push_back(move(E));
            if (small_if)
                break;
        }
        else {
            if (CurTok == tok_end) {
                break;
            }
            return nullptr;
        }
    }
    if (!small_if) {
        if (CurTok != tok_end)
            return LogError("expected end");

        getNextToken();
    }
    if (CurTok == tok_else) {
        getNextToken(); // eat else
        auto Else = ParseExpression();
        if (!Else)
            return nullptr;

        return llvm::make_unique<IfExprAST>(move(Cond), move(then), move(Else));
    }
    return llvm::make_unique<IfExprAST>(move(Cond), move(then));
}

unique_ptr<ExprAST> ParseForExpr() {
    getNextToken();  // eat the for.

    if (CurTok != tok_identifier)
        return LogError("expected identifier after for");

    string IdName = IdentifierStr;
    getNextToken();  // eat identifier.

    if (CurTok != tok_ass) {
        return LogError("expected ':=' after for");
    }
    getNextToken();  // eat ':='.


    auto Start = ParseExpression();
    if (!Start)
        return nullptr;

    bool downto = false;
    if (CurTok == tok_downto) {
        downto = true;
    }
    else {
        if (CurTok != tok_to) {
            return LogError("expected 'to' after for");
        }
    }
    getNextToken();  // eat 'to'.

    auto End = ParseExpression();
    if (!End)
        return nullptr;

    unique_ptr<ExprAST> Step;
    if (CurTok != tok_do)
        return LogError("expected 'in' after for");
    getNextToken();  // eat do.

    if (CurTok != tok_beg)
        return LogError("expected 'begin' after for");
    getNextToken();  // eat begin.

    vector<unique_ptr<ExprAST>> body;

    while (CurTok != tok_end) {
        if (auto E = ParseExpression()) {
            if (CurTok == ';')
                getNextToken();
            body.push_back(move(E));
        }
        else {
            if (CurTok == tok_end) {
                getNextToken();
                return llvm::make_unique<ForExprAST>(IdName, move(Start), move(End), move(Step), move(body), downto);
            }
            return nullptr;
        }
    }
    if (CurTok == tok_end)
        getNextToken();
    return llvm::make_unique<ForExprAST>(IdName, move(Start), move(End), move(Step), move(body), downto);
}
//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto * FnIR = FnAST->codegen()) {
            fprintf(stderr, "Read function definition\n");
//            TheModule->print(errs(), nullptr);

            /*FnIR->print(errs());
            fprintf(stderr, "\n");*/
        }
    }
    else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        if (auto * FnIR = ProtoAST->codegen()) {
            fprintf(stderr, "Read extern\n ");
            /*FnIR->print(errs());
            fprintf(stderr, "\n");*/
        }
    }
    else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void HandleTopLevelExpression() {

    // Evaluate a top-level expression into an anonymous function.
    if (auto FnAST = ParseTopLevelExpr()) {
        FnAST->codegen();

    }
    else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void MainLoop() {
    while (true) {
        switch (CurTok) {
            case tok_eof:
                return;
            case ';': // ignore top-level ;
                getNextToken();
                break;
            case '.': // end
                return;
            case tok_end: // ignore top-level end.
                getNextToken();
                break;
            case tok_func:
                HandleDefinition();
                break;
            case tok_procedure:
                HandleDefinition();
                break;
            case tok_extern:
                HandleExtern();
                break;
            case tok_var:
                HandleGlobalVar();
                break;
            case tok_const:
                HandleConst();
                break;
            default:
                HandleTopLevelExpression();
                break;
        }
//        fprintf(stderr, "ready> ");
    }
}

unique_ptr<ExprAST> ParseVarExpr() {
    getNextToken(); // eat the var.

    vector<pair<string, unique_ptr<ExprAST>>> VarNames;

    // At least one variable name is required.
    if (CurTok != tok_identifier)
        return LogError("expected identifier after var");

    while (CurTok == tok_identifier) {
        string Name = IdentifierStr;
        getNextToken(); // eat identifier.

        // Read the optional initializer.
        unique_ptr<ExprAST> Init = nullptr;

        VarNames.emplace_back(Name, move(Init));
        swallow_int(false);
        // End of var list, exit loop.
        if (CurTok != ',')
            break;
        getNextToken(); // eat the ','.

        if (CurTok != tok_identifier)
            return LogError("expected identifier list after var");
    }

    if (CurTok != ';')
        return LogError("expected ';' keyword after 'var'");
    getNextToken(); // eat ';'.

    return llvm::make_unique<VarExprAST>(move(VarNames));
}


unique_ptr<ExprAST> ParseConstExpr() {
    getNextToken(); // eat the var.

    vector<pair<string, unique_ptr<ExprAST>>> VarNames;

    // At least one variable name is required.
    if (CurTok != tok_identifier)
        return LogError("expected identifier after var");

    while (CurTok == tok_identifier) {
        string Name = IdentifierStr;
        constants.insert(IdentifierStr);
        getNextToken(); // eat identifier.

        // Read the optional initializer.
        unique_ptr<ExprAST> Init = nullptr;
        if (CurTok == '=') {
            getNextToken(); // eat the '='.

            Init = ParseExpression();
            if (!Init)
                return nullptr;
        }
        else {
            return LogError("NOT INITIALIZED");
        }

        VarNames.emplace_back(Name, move(Init));

        // End of var list, exit loop.
        if (CurTok != ';')
            break;
        getNextToken(); // eat the ','.

        if (CurTok == tok_func || CurTok == tok_beg)
            break;
        if (CurTok != tok_identifier)
            return LogError("expected identifier list after var");
    }

    return llvm::make_unique<ConstExprAST>(move(VarNames));
}

unique_ptr<ExprAST> ParseUnary() {
    // If the current token is not an operator, it must be a primary expr.
    if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
        return ParsePrimary();

    // If this is a unary operator, read it.
    int Opc = CurTok;
    getNextToken();
    if (auto Operand = ParseUnary())
        return llvm::make_unique<UnaryExprAST>(Opc, move(Operand));
    return nullptr;
}

void HandleGlobalVar() {
    if (auto FnAST = ParseVarExpr()) {
        if (auto result = FnAST->emmit_global()) {
            fprintf(stderr, "Read function definition\n");
//            TheModule->print(errs(), nullptr);
//            fprintf(stderr, "\n");
        }
    }
    else {
        // Skip token for error recovery.
        getNextToken();
    }
}

void HandleConst() {
    if (auto FnAST = ParseConstExpr()) {
        if (auto result = FnAST->emmit_global()) {
            fprintf(stderr, "Read function definition\n");
//            TheModule->print(errs(), nullptr);
//            fprintf(stderr, "\n");
        }
    }
    else {
        // Skip token for error recovery.
        getNextToken();
    }
}
