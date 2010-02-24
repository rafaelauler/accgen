//===- main.cpp   - Test program                          --*- C++ -*------===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// Test program for using the rules/semantic parser.
//
//===----------------------------------------------------------------------===//

#include "../InsnSelector/TransformationRules.h"
#include "../InsnSelector/Semantic.h"

using namespace backendgen;
using namespace backendgen::expression;

extern TransformationRules RuleManager;
extern InstrManager InstructionManager;
extern OperandTableManager OperandTable;

int yyparse();

int
main()
{
  int ret = yyparse();

  RuleManager.print(std::cout);

  InstructionManager.printAll(std::cout);

  OperandTable.printAll(std::cout);

  return ret;
}
