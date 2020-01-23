
//
// Created by Ustynov, Artem on 2019-05-04.
//
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#include <utility>
#include <string>
#include <vector>
#include "parser.h"
#include "ExprAST.h"

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

LLVMContext TheContext;
IRBuilder<> Builder(TheContext);
unique_ptr<Module> TheModule;
map<string, AllocaInst *> NamedValues;
map<string, unique_ptr<PrototypeAST>> FunctionProtos;
set<string> constants;

Value * LogErrorV(const char * Str) {
    LogError(Str);
    return nullptr;
}

Function * getFunction(const string & Name) {
    // First, see if the function has already been added to the current module.
    if (auto * F = TheModule->getFunction(Name))
        return F;

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end())
        return FI->second->codegen();

    // If no existing prototype exists, return null.
    return nullptr;
}

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
AllocaInst * CreateEntryBlockAlloca(Function * TheFunction, const string & VarName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());

    return TmpB.CreateAlloca(Type::getInt32Ty(TheContext), nullptr, VarName);
}

Value * NumberExprAST::codegen() {
    return ConstantInt::get(TheContext, APInt(32, Val, true));
}

Value * VariableExprAST::codegen() {
    // Look this variable up in the function.
    Value * V = NamedValues[Name];
    if (!V)
        V = TheModule->getNamedGlobal(Name);

    if (!V)
        return LogErrorV("Unknown variable name");

    // Load the value.
    return Builder.CreateLoad(V, Name.c_str());
}

const std::string VariableExprAST::getName() const { return Name; }

Value * BinaryExprAST::codegen() {
    // Special case '=' because we don't want to emit the LHS as an expression.
    if (Op == '=') {
        // Assignment requires the LHS to be an identifier.
        // This assume we're building without RTTI because LLVM builds that way by
        // default.  If you build LLVM with RTTI this can be changed to a
        // dynamic_cast for automatic error checking.
        auto * LHSE = static_cast<VariableExprAST *>(LHS.get());
        if (!LHSE)
            return LogErrorV("destination of '=' must be a variable");
        // Codegen the RHS.
        Value * Val = RHS->codegen();
        if (!Val)
            return nullptr;

        // Look up the name.
        if (constants.find(LHSE->getName()) != constants.end()) {
            cout << "\nCONSTANTS are no-no" << endl;
            return LogErrorV("CONSTANTS are no-no");
        }
        Value * Variable = NamedValues[LHSE->getName()];
        if (!Variable)
            Variable = TheModule->getNamedGlobal(LHSE->getName());
        if (!Variable)
            return LogErrorV("Unknown variable name");

        Builder.CreateStore(Val, Variable);
        TheModule->print(errs(), nullptr);

        return Val;
    }

    Value * L = LHS->codegen();
    Value * R = RHS->codegen();
    if (!L || !R)
        return nullptr;

    switch (Op) {
        /*case ';':
            return R;*/
        case '+':
            return Builder.CreateAdd(L, R, "addtmp");
        case '-':
            return Builder.CreateSub(L, R, "subtmp");
        case '*':
            return Builder.CreateMul(L, R, "multmp");
        case '/':
            return Builder.CreateSDiv(L, R, "divtmp");
        case '<':
            L = Builder.CreateICmpSLT(L, R, "lttmp");
            return Builder.CreateIntCast(L, Type::getInt32Ty(TheContext), true, "booltmp");
        case tok_le:
            L = Builder.CreateICmpSLE(L, R, "lttmp");
            return Builder.CreateIntCast(L, Type::getInt32Ty(TheContext), true, "booltmp");
        case '>':
            L = Builder.CreateICmpSGT(L, R, "lttmp");
            return Builder.CreateIntCast(L, Type::getInt32Ty(TheContext), true, "booltmp");
        case tok_ge:
            L = Builder.CreateICmpSGE(L, R, "lttmp");
            return Builder.CreateIntCast(L, Type::getInt32Ty(TheContext), true, "booltmp");
        case tok_eq:
            L = Builder.CreateICmpEQ(L, R, "lttmp");
            return Builder.CreateIntCast(L, Type::getInt32Ty(TheContext), true, "booltmp");
        case tok_ne:
            L = Builder.CreateICmpNE(L, R, "lttmp");
            return Builder.CreateIntCast(L, Type::getInt32Ty(TheContext), true, "booltmp");
        default:
            break;
    }

    // If it wasn't a builtin binary operator, it must be a user defined one. Emit
    // a call to it.
    Function * F = getFunction(std::string("binary") + Op);
    assert(F && "binary operator not found!");

    Value * Ops[] = {L, R};
    return Builder.CreateCall(F, Ops, "binop");
}

Value * CallExprAST::codegen() {
    // Look up the name in the global module table.
    Function * CalleeF = getFunction(Callee);
    if (!CalleeF)
        return LogErrorV("Unknown function referenced");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    vector<Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        if (CalleeF->getName() == "readln") {

            Value * V = NamedValues[Args[i]->getName()];
            if (!V)
                V = TheModule->getNamedGlobal(Args[i]->getName());
            if (!V)
                return LogErrorV("Unknown variable name");

            Builder.CreateLoad(Builder.CreateIntToPtr(V, Type::getInt32PtrTy(TheContext)), "ptr");
            ArgsV.push_back(Builder.CreateIntToPtr(V, Type::getInt32PtrTy(TheContext)));

            //ArgsV.push_back(Builder.CreateIntToPtr(Args[i]->codegen(), Type::getInt32PtrTy(TheContext)));
            if (!ArgsV.back())
                return nullptr;
        }
        else {
            ArgsV.push_back(Args[i]->codegen());

            if (!ArgsV.back())
                return nullptr;
        }
    }
//    TheModule->print(errs(), nullptr);

    //auto ret =
    if (CalleeF->getReturnType()->isVoidTy())
        return Builder.CreateCall(CalleeF, ArgsV);
    return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Function * PrototypeAST::codegen() {
    // Make the function type:  double(double,double) etc.
    vector<Type *> Ints(Args.size(), Type::getInt32Ty(TheContext));

    FunctionType * FT;
    if (procedure)
        FT = FunctionType::get(Type::getVoidTy(TheContext), Ints, false);
    else FT = FunctionType::get(Type::getInt32Ty(TheContext), Ints, false);
    Function * F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());
    // Set names for all arguments.
    unsigned Idx = 0;
    for (auto & Arg : F->args())
        Arg.setName(Args[Idx++]);
    return F;
}

void createReadln() {
    // Make the function type:  double(double,double) etc.
    vector<Type *> Ints(1, Type::getInt32PtrTy(TheContext));

    FunctionType * FT = FunctionType::get(Type::getInt32Ty(TheContext), Ints, false);
    Function * F = Function::Create(FT, Function::ExternalLinkage, "readln", TheModule.get());
    // Set names for all arguments.
    for (auto & Arg : F->args())
        Arg.setName("x");
}

void createWriteln() {
    // Make the function type:  double(double,double) etc.
    vector<Type *> Ints(1, Type::getInt32Ty(TheContext));

    FunctionType * FT = FunctionType::get(Type::getInt32Ty(TheContext), Ints, false);
    Function * F = Function::Create(FT, Function::ExternalLinkage, "writeln", TheModule.get());
    // Set names for all arguments.
    for (auto & Arg : F->args())
        Arg.setName("x");
}

bool PrototypeAST::isUnaryOp() const { return IsOperator && Args.size() == 1; }

bool PrototypeAST::isBinaryOp() const { return IsOperator && Args.size() == 2; }

char PrototypeAST::getOperatorName() const {
    assert(isUnaryOp() || isBinaryOp());
    return Name[Name.size() - 1];
}

unsigned PrototypeAST::getBinaryPrecedence() const { return Precedence; }

const string & PrototypeAST::getName() const { return Name; }

Function * FunctionAST::codegen() {
    // Transfer ownership of the prototype to the FunctionProtos map, but keep a
    // reference to it for use below.
    auto & P = *Proto;
    FunctionProtos[Proto->getName()] = move(Proto);
    Function * TheFunction = getFunction(P.getName());
    if (!TheFunction)
        return nullptr;

    // If this is an operator, install it.
    if (P.isBinaryOp())
        BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

    // Create a new basic block to start insertion into.
    BasicBlock * BB;
    if (TheFunction->begin() == TheFunction->end()) {
        BB = BasicBlock::Create(TheContext, "entry", TheFunction);
        Builder.SetInsertPoint(BB);
    }
    else Builder.SetInsertPoint(&*prev(TheFunction->end()));

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto & Arg : TheFunction->args()) {
        // Create an alloca for this variable.
        AllocaInst * Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

        // Store the initial value into the alloca.
        Builder.CreateStore(&Arg, Alloca);

        // Add arguments to variable symbol table.
        NamedValues[Arg.getName()] = Alloca;
    }


    for (int i = 0; i < Body.size(); i++) {
        if (auto RetVal = Body[i]->codegen()) {
            // Finish off the function.
            if (P.getName() != "main" && i == Body.size() - 1 && !procedure) {
                Builder.CreateRet(RetVal);
            }
            if (P.getName() != "main" && i == Body.size() - 1 && procedure)
                Builder.CreateRet(nullptr);

            // Validate the generated code, checking for consistency.
            verifyFunction(*TheFunction);
        }
        else {
            TheFunction->eraseFromParent();
            if (P.isBinaryOp())
                BinopPrecedence.erase(P.getOperatorName());
            return nullptr;
        }
    }
    return TheFunction;
    // Error reading body, remove function.

}

Value * ForExprAST::codegen() {
    Function * TheFunction = Builder.GetInsertBlock()->getParent();

    // Create an alloca for the variable in the entry block.
    AllocaInst * Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

    // Emit the start code first, without 'variable' in scope.
    Value * StartVal = Start->codegen();
    if (!StartVal)
        return nullptr;

    // Store the value into the alloca.
    Builder.CreateStore(StartVal, Alloca);

    // Make the new basic block for the loop header, inserting after current
    // block.
    BasicBlock * LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);

    // Insert an explicit fall through from the current block to the LoopBB.
    Builder.CreateBr(LoopBB);

    // Start insertion in LoopBB.
    Builder.SetInsertPoint(LoopBB);

    // Within the loop, the variable is defined equal to the PHI node.  If it
    // shadows an existing variable, we have to restore it, so save it now.
    AllocaInst * OldVal = NamedValues[VarName];
    NamedValues[VarName] = Alloca;

    // Emit the body of the loop.  This, like any other expr, can change the
    // current BB.  Note that we ignore the value computed by the body, but don't
    // allow an error.
    for (const auto & body: Body) {
        if (!body->codegen())
            return nullptr;
    }
    // Emit the step value.
    Value * StepVal = nullptr;
    if (Step) {
        StepVal = Step->codegen();
        if (!StepVal)
            return nullptr;
    }
    else {
        // If not specified, use 1.0.
        StepVal = ConstantInt::get(TheContext, APInt(32, 1, true));
    }

    // Compute the end condition.
    Value * EndCond = End->codegen();
    if (!EndCond)
        return nullptr;

    // Reload, increment, and restore the alloca.  This handles the case where
    // the body of the loop mutates the variable.
    Value * CurVar = Builder.CreateLoad(Alloca, VarName.c_str());
    Value * NextVar;
    if (!downto)
        NextVar = Builder.CreateAdd(CurVar, StepVal, "nextvar");
    else
        NextVar = Builder.CreateSub(CurVar, StepVal, "nextvar");

    Builder.CreateStore(NextVar, Alloca);

    // Convert condition to a bool by comparing non-equal to 0.0.
    EndCond = Builder.CreateICmpNE(EndCond, CurVar, "loopcond");

    // Create the "after loop" block and insert it.
    BasicBlock * AfterBB = BasicBlock::Create(TheContext, "afterloop", TheFunction);

    // Insert the conditional branch into the end of LoopEndBB.
    Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

    // Any new code will be inserted in AfterBB.
    Builder.SetInsertPoint(AfterBB);

    // Restore the unshadowed variable.
    if (OldVal)
        NamedValues[VarName] = OldVal;
    else
        NamedValues.erase(VarName);

    // for expr always returns 0.0.
    return Constant::getNullValue(Type::getInt32Ty(TheContext));
}

Value * IfExprAST::codegen() {
    Value * CondV = Cond->codegen();
    if (!CondV)
        return nullptr;
    //TheModule->print(errs(), nullptr);

    // Convert condition to a bool by comparing non-equal to 0.0.
    CondV = Builder.CreateICmpNE(CondV, ConstantInt::get(TheContext, APInt(32, 0, true)), "ifcond");

    Function * TheFunction = Builder.GetInsertBlock()->getParent();

    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    BasicBlock * ThenBB = BasicBlock::Create(TheContext, "then", TheFunction);

    BasicBlock * ElseBB;
    if (!no_else)
        ElseBB = BasicBlock::Create(TheContext, "else");
    BasicBlock * MergeBB = BasicBlock::Create(TheContext, "ifcont");

    if (no_else)
        Builder.CreateCondBr(CondV, ThenBB, MergeBB);
    else Builder.CreateCondBr(CondV, ThenBB, ElseBB);

    // Emit then value.
    Builder.SetInsertPoint(ThenBB);

    Value * ThenV;
    for (const auto & then: Then) {
        ThenV = then->codegen();
        if (!ThenV)
            return nullptr;
    }

    Builder.CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = Builder.GetInsertBlock();

    // Emit else block.i
    if (!no_else) {
        TheFunction->getBasicBlockList().push_back(ElseBB);
        Builder.SetInsertPoint(ElseBB);
    }

    Value * ElseV;
    if (!no_else) {
        ElseV = Else->codegen();
        if (!ElseV)
            return nullptr;
    }
    if (!no_else)
        Builder.CreateBr(MergeBB);
    // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
    ElseBB = Builder.GetInsertBlock();

    // Emit merge block.
    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
    PHINode * PN = Builder.CreatePHI(Type::getInt32Ty(TheContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    TheModule->print(errs(), nullptr);

    if (!no_else)
        //    PN->addIncoming(ThenV, ElseBB);
        PN->addIncoming(ElseV, ElseBB);
    return PN;
}

Value * VarExprAST::codegen() {
    std::vector<AllocaInst *> OldBindings;

    Function * TheFunction = Builder.GetInsertBlock()->getParent();

    // Register all variables and emit their initializer.
    for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
        const std::string & VarName = VarNames[i].first;
        ExprAST * Init = VarNames[i].second.get();

        // Emit the initializer before adding the variable to scope, this prevents
        // the initializer from referencing the variable itself, and permits stuff
        Value * InitVal;
        if (Init) {
            InitVal = Init->codegen();
            if (!InitVal)
                return nullptr;
        }
        else { // If not specified, use 0.0.
            InitVal = ConstantInt::get(TheContext, APInt(32, 0, true));
        }

        AllocaInst * Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
        Builder.CreateStore(InitVal, Alloca);

        // Remember the old variable binding so that we can restore the binding when
        // we unrecurse.
        OldBindings.push_back(NamedValues[VarName]);

        // Remember this binding.
        NamedValues[VarName] = Alloca;
    }

    // Return the body computation.
    return TheFunction;
}

Value * ConstExprAST::codegen() {
    std::vector<AllocaInst *> OldBindings;

    Function * TheFunction = Builder.GetInsertBlock()->getParent();

    // Register all variables and emit their initializer.
    for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
        const std::string & VarName = VarNames[i].first;
        ExprAST * Init = VarNames[i].second.get();

        // Emit the initializer before adding the variable to scope, this prevents
        // the initializer from referencing the variable itself, and permits stuff
        Value * InitVal;
        if (Init) {
            InitVal = Init->codegen();
            if (!InitVal)
                return nullptr;
        }
        else { // If not specified, use 0.0.
            InitVal = ConstantInt::get(TheContext, APInt(32, 0, true));
        }

        AllocaInst * Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
        Builder.CreateStore(InitVal, Alloca);

        // Remember the old variable binding so that we can restore the binding when
        // we unrecurse.
        OldBindings.push_back(NamedValues[VarName]);

        // Remember this binding.
        NamedValues[VarName] = Alloca;
    }

    // Return the body computation.
    return TheFunction;
}

bool VarExprAST::emmit_global() {
    for (auto & var : VarNames) {
        TheModule->getOrInsertGlobal(var.first, Builder.getInt32Ty());
        GlobalVariable * gvar = TheModule->getNamedGlobal(var.first);
        gvar->setLinkage(GlobalValue::ExternalLinkage);
        gvar->setInitializer(ConstantInt::get(TheContext, APInt(32, 0, true)));
        // gvar->setAlignment(4);
    }
    return true;
}

bool ConstExprAST::emmit_global() {
    int i = 0;
    for (auto & var : VarNames) {
        TheModule->getOrInsertGlobal(var.first, Builder.getInt32Ty());
        GlobalVariable * gvar = TheModule->getNamedGlobal(var.first);
        gvar->setLinkage(GlobalValue::ExternalLinkage);
        ExprAST * Init = VarNames[i].second.get();
        Value * InitVal;
        if (Init) {
            InitVal = Init->codegen();
            if (!InitVal)
                return false;
        }
        gvar->setInitializer(dyn_cast<llvm::ConstantInt>(InitVal));
        // gvar->setAlignment(4);
        i++;
    }
    return true;
}

Value * UnaryExprAST::codegen() {
    Value * OperandV = Operand->codegen();
    if (!OperandV)
        return nullptr;

    Function * F = getFunction(std::string("unary") + Opcode);
    if (!F)
        return LogErrorV("Unknown unary operator");

    return Builder.CreateCall(F, OperandV, "unop");
}
