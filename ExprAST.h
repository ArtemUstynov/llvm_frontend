#include <utility>

#include <utility>


//
// Created by Ustynov, Artem on 2019-05-04.
//
#ifndef COMP_AST_H
#define COMP_AST_H

#include <utility>
#include <string>
#include <vector>
#include "llvm/IR/Value.h"
#include "llvm/IR/IRBuilder.h"
#include <utility>
#include <set>

using namespace std;
using namespace llvm;

extern LLVMContext TheContext;
extern IRBuilder<> Builder;
extern unique_ptr<Module> TheModule;
extern map<string, AllocaInst *> NamedValues;

class PrototypeAST;

class FunctionAST;

extern map<string, unique_ptr<PrototypeAST>> FunctionProtos;
extern set<string> constants;

Function * getFunction(const string & Name);

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
AllocaInst * CreateEntryBlockAlloca(Function * TheFunction, const string & VarName);

/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
    virtual ~ExprAST() = default;

    virtual Value * codegen() = 0;

    virtual bool emmit_global() { return true; };

    virtual const string getName() const { return "err__none__implemented"; }
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
    int Val;

public:
    NumberExprAST(int Val) : Val(Val) {}

    virtual Value * codegen() override;

};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
    string Name;

public:
    virtual Value * codegen() override;

    VariableExprAST(string Name) : Name(move(Name)) {}

    const string getName() const override;

};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
    char Op;
    unique_ptr<ExprAST> LHS, RHS;

public:
    virtual Value * codegen();

    BinaryExprAST(char op, unique_ptr<ExprAST> LHS, unique_ptr<ExprAST> RHS)
            : Op(op), LHS(move(LHS)), RHS(move(RHS)) {}
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
    string Callee;
    vector<unique_ptr<ExprAST>> Args;

public:
    CallExprAST(string Callee, vector<unique_ptr<ExprAST>> Args)
            : Callee(move(Callee)), Args(move(Args)) {}

    virtual Value * codegen();

};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
    string Name;
    vector<string> Args;
    bool IsOperator;
    unsigned Precedence; // Precedence if a binary op.

public:
    bool procedure;

    PrototypeAST(string Name, vector<string> Args, bool IsOperator = false, unsigned Prec = 0, bool procedure = false)
            : Name(move(Name)), Args(move(Args)), IsOperator(IsOperator), Precedence(Prec), procedure(procedure) {}

    const string & getName() const;

    Function * codegen();

    bool isUnaryOp() const;

    bool isBinaryOp() const;

    char getOperatorName() const;

    unsigned getBinaryPrecedence() const;
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
    unique_ptr<PrototypeAST> Proto;
    vector<unique_ptr<ExprAST>> Body;

public:
    bool procedure;

//    FunctionAST(unique_ptr<PrototypeAST> Proto, vector<unique_ptr<ExprAST>> Body) : Proto(move(Proto)), Body(move(Body)) {}

    FunctionAST(unique_ptr<PrototypeAST> Proto, vector<unique_ptr<ExprAST>> Body, bool procedure = false)
            : Proto(move(Proto)), Body(move(Body)), procedure(procedure) {}

    Function * codegen();

};

class IfExprAST : public ExprAST {
    unique_ptr<ExprAST> Cond, Else;
    vector<unique_ptr<ExprAST>> Then;
public:
    bool no_else;

    IfExprAST(unique_ptr<ExprAST> Cond, vector<unique_ptr<ExprAST>> Then, unique_ptr<ExprAST> Else)
            : Cond(move(Cond)), Then(move(Then)), Else(move(Else)) {}

    IfExprAST(unique_ptr<ExprAST> Cond, vector<unique_ptr<ExprAST>> Then)
            : Cond(move(Cond)), Then(move(Then)) { no_else = true; }

    Value * codegen() override;
};

class ForExprAST : public ExprAST {
    string VarName;
    unique_ptr<ExprAST> Start, End, Step;
    vector<unique_ptr<ExprAST>> Body;
    bool downto;
public:
    ForExprAST(string VarName, unique_ptr<ExprAST> Start, unique_ptr<ExprAST> End, unique_ptr<ExprAST> Step, vector<unique_ptr<ExprAST>> Body,
               bool downto)
            : VarName(move(VarName)), Start(move(Start)), End(move(End)), Step(move(Step)), Body(move(Body)), downto(downto) {}

    Value * codegen() override;
};

/// VarExprAST - Expression class for var/in
class VarExprAST : public ExprAST {
    vector<pair<string, unique_ptr<ExprAST>>> VarNames;
public:
    VarExprAST(vector<pair<string, unique_ptr<ExprAST>>> VarNames) : VarNames(move(VarNames)) {}

    Value * codegen() override;

    bool emmit_global() override;
};

/// returnAST - Expression class for var/in

class ConstExprAST : public ExprAST {
    vector<pair<string, unique_ptr<ExprAST>>> VarNames;
public:
    ConstExprAST(vector<pair<string, unique_ptr<ExprAST>>> VarNames) : VarNames(move(VarNames)) {}

    Value * codegen() override;

    bool emmit_global() override;
};

/// UnaryExprAST - Expression class for a unary operator.
class UnaryExprAST : public ExprAST {
    char Opcode;
    unique_ptr<ExprAST> Operand;

public:
    UnaryExprAST(char Opcode, unique_ptr<ExprAST> Operand) : Opcode(Opcode), Operand(move(Operand)) {}

    Value * codegen() override;
};

void createReadln();

void createWriteln();


#endif