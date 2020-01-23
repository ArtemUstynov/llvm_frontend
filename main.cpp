//
// Created by Ustynov, Artem on 2019-05-03.
//

#include "parser.h"
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <llvm/IR/Verifier.h>
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"

#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"
//WAS INSPIRED BY LLVM OFFICIAL TUTORIAL http://llvm.org/docs/tutorial/index.html
int main() {
    // Install standard binary operators.
    // 1 is lowest precedence.
//     BinopPrecedence[';'] = 1;
    BinopPrecedence['='] = 2;
    BinopPrecedence['<'] = 10;
    BinopPrecedence['>'] = 10;
    BinopPrecedence[tok_le] = 10;
    BinopPrecedence[tok_ge] = 10;
    BinopPrecedence[tok_eq] = 10;
    BinopPrecedence[tok_ne] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;
    BinopPrecedence['/'] = 40; // highest.

    // Prime the first token.
//    fprintf(stderr, "ready> ");
    for (int i = 0; i < 4; i++) /// !!!!!!!!!!!!!!!! FOR PROGRAM NAME WHICH IS GARBAGE
        getNextToken();
    TheModule = llvm::make_unique<Module>("oh ye jit", TheContext);

    createReadln();
    createWriteln();
    MainLoop();

    // Initialize the target registry etc.
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    auto TargetTriple = sys::getDefaultTargetTriple();
    TheModule->setTargetTriple(TargetTriple);

    std::string Error;
    auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);

    // Print an error and exit if we couldn't find the requested target.
    // This generally occurs if we've forgotten to initialise the
    // TargetRegistry or we have a bogus target triple.
    if (!Target) {
        errs() << "FAK" << Error;
        return 1;
    }

    auto CPU = "generic";
    auto Features = "";


    Function * function = getFunction("main");
    Builder.CreateRet(Builder.getInt32(0));
    verifyFunction(*function);

    TargetOptions opt;
    auto RM = Optional<Reloc::Model>();
    auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

    TheModule->setDataLayout(TheTargetMachine->createDataLayout());

    auto Filename = "a.o";
    std::error_code EC;
    raw_fd_ostream dest(Filename, EC, sys::fs::F_None);

    if (EC) {
        errs() << "Could not open file: " << EC.message();
        return 1;
    }

    legacy::PassManager pass;
    auto FileType = TargetMachine::CGFT_ObjectFile;

    if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        errs() << "TheTargetMachine can't emit a file of this type";
        return 1;
    }
    //  TheModule->print(errs(), nullptr);

    // Promote allocas to registers.
    pass.add(createPromoteMemoryToRegisterPass());
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    pass.add(createInstructionCombiningPass());
    // Reassociate expressions.
    pass.add(createReassociatePass());
    // Reassociate expressions.
    pass.add(createJumpThreadingPass());
    pass.add(createCFGSimplificationPass());
    TheModule->print(errs(), nullptr);
    pass.run(*TheModule);
    dest.flush();

   // outs() << "Wrote " << Filename << "\n";


    system("clang  a.o f.c -o a.out");

    return 0;
}
