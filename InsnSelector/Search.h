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

  typedef std::list<const std::string*> NameListType;
  typedef std::list<NameListType*> OperandsDefsType;
  typedef std::list<const Instruction*> InstrList;

  // This struct stores a result. This is a list of instructions
  // implementing the requested expression and its cost information.
  struct SearchResult {
    InstrList *Instructions;
    CostType Cost;
    OperandsDefsType *OperandsDefs;
    SearchResult();
    ~SearchResult();
  };

  const unsigned MAXDEPTH = 8;

  // Main interface for search algorithms
  class Search {
    TransformationRules& RulesMgr;    
    InstrManager& InstructionsMgr;
    inline bool HasCloseSemantic(unsigned InstrPO, unsigned ExpPO);
    SearchResult* TransformExpression(const Tree* Expression,
				      const Tree* InsnSemantic, 
				      unsigned CurDepth);
    SearchResult* ApplyDecompositionRule(const Rule *R, const Tree* Expression,
					 const Tree* Goal, Tree *& MatchedGoal,
					 unsigned CurDepth);
    bool TransformExpressionAux(const Tree* Transformed,
				const Tree* InsnSemantic, SearchResult* Result,
				unsigned CurDepth);
  public:
    Search(TransformationRules& RulesMgr, InstrManager& InstructionsMgr);
    SearchResult* operator() (const Tree* Expression, unsigned CurDepth);
  };

}








#endif
