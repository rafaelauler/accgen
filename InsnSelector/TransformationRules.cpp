//===- TransformationRules.h                                    --*- C++ -*===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// These classes represent the instruction behaviour, as perceived by the
// compiler backend generator tool.
//
//===----------------------------------------------------------------------===//


#include "TransformationRules.h"
#include <iostream>

namespace backendgen {

  bool TransformationRules::createRule(expression::Tree* LHS,
				       expression::Tree* RHS,
				       bool Equivalence) {
    Rule newRule;
    newRule.LHS = LHS;
    newRule.RHS = RHS;
    newRule.Equivalence = Equivalence;

    Rules.push_back(newRule);
  }

  void TransformationRules::print() {
    std::cout << "Rules Manager has a total of " << Rules.size()
	      << " rule(s).\n";
    std::cout << "==================================\n";
    for (std::list<Rule>::const_iterator I = Rules.begin(), E = Rules.end();
	 I != E; ++I)
      {
	I->LHS->print();
	if (I->Equivalence)
	  std::cout << "<=> ";
	else
	  std::cout << "=> ";
	I->RHS->print();
	std::cout << ";\n";
      }
    std::cout << "\n";
  }
  
  TransformationRules::~TransformationRules() {
    for (std::list<Rule>::const_iterator I = Rules.begin(), E = Rules.end();
	 I != E; ++I)
      {
	delete I->LHS;
	delete I->RHS;
      }
  }

  RuleIterator TransformationRules::getBegin() {
    return Rules.begin();
  }

  RuleIterator TransformationRules::getEnd() {
    return Rules.end();
  }

} // end namespace backendgen
