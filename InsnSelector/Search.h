//===- Search.h - Header file                             --*- C++ -*------===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// This is where all magic happens, i.e. the engine of the AI search algorithm
// for implementation of patterns and computable expressions.
//
//===----------------------------------------------------------------------===//
#ifndef SEARCH_H
#define SEARCH_H

#include "TransformationRules.h"
#include "Semantic.h"
#include <list>

using namespace backendgen::expression;

namespace backendgen {

  // Main interface for search algorithms
  class Search {
    TransformationRules& RulesMgr;    
    InstrManager& InstructionsMgr;
  public:
    Search(TransformationRules& RulesMgr, InstrManager& InstructionsMgr);
    std::list<Instruction*>* operator() (Tree* Expression);
  };

}








#endif
