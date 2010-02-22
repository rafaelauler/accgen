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

using namespace backendgen;

extern TransformationRules RuleManager;

int yyparse();

int
main()
{
  int ret = yyparse();

  RuleManager.print();

  return ret;
}
