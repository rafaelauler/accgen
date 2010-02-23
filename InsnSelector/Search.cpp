//===- Search.cpp - Implementation                         --*- C++ -*------===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// This is where all magic happens, i.e. the engine of the AI search algorithm
// for implementation of patterns and computable expressions.
//
//===----------------------------------------------------------------------===//

#include "Search.h"  

namespace backendgen {

  // Search member functions

  // Constructor
  Search::Search(TransformationRules& RulesMgr, 
		 InstrManager& InstructionsMgr):
    RulesMgr(RulesMgr), InstructionsMgr(InstructionsMgr)
  { }


  // Auxiliary function returns true if two trees are directly
  // equivalent
  bool Compare (Tree *E1, Tree *E2) 
  {
    bool isOperator;
    if ((isOperator = E1->isOperator()) == E2->isOperator() &&
	E1->getType() == E2->getType())
      {
	if (!isOperator) {
	  return true;
	}
	
	bool match = true;
	// Now we know we are handling with operators and may safely cast
	// and analyze its children
	Operator *O1 = dynamic_cast<Operator*>(E1),
	  *O2 = dynamic_cast<Operator*>(E2);
	for (int I = 0, E = O1->getArity(); I != E; ++I) {
	  if (!Compare((*O1)[I], (*O2)[I])) {
	    match = false;
	    break;
	  }
	}
	return match;
      }
    return false;
  }

  // This operator overload effectively starts the search
  std::list<Instruction*>* Search::operator() (Tree* Expression)
  {
    std::list<Instruction*>* Result = new std::list<Instruction*>();
    // First, see if this expression is directly computable by
    // an available instruction, or part of this instruction    
    for (InstrIterator I = InstructionsMgr.getBegin(), 
	   E = InstructionsMgr.getEnd(); I != E; ++I) 
      {
        // compare each semantic expression
      }
  }

}
