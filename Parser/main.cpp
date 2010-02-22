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

int yyparse();

int
main()
{
  int ret = yyparse();

  RuleManager.print();

  InstructionManager.printAll();

  return ret;
}
