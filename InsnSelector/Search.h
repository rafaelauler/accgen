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
#include "../Instruction.h"
#include <list>

using namespace backendgen::expression;

namespace backendgen {

  typedef std::list<std::string> NameListType;
  typedef std::list<NameListType*> OperandsDefsType;
  // This pair stores the instruction and the specific semantic of that
  // instruction that matched our purposes in the search. (Remember
  // an instruction may have many semantic trees describing its behaviour).
  typedef std::list<std::pair<const Instruction*, SemanticIterator> > InstrList;

  // This struct stores a result. This is a list of instructions
  // implementing the requested expression and its cost information.
  // Also, an associated list of operands definitions is available.
  // For each instruction, there is a list of operands definitions with
  // names to use as the operands of this instruction. This implies that
  // in complete search results, the list of operandsdefs is of the same
  // size of the list of instructions.
  struct SearchResult {
    InstrList *Instructions;    
    CostType Cost;
    OperandsDefsType *OperandsDefs;    
    SearchResult();
    ~SearchResult();
  };

  // Main interface for search algorithms
  class Search {
    TransformationRules& RulesMgr;    
    InstrManager& InstructionsMgr;

    unsigned MaxDepth;

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
    unsigned getMaxDepth() { return MaxDepth; }
    void setMaxDepth(unsigned MaxDepth) { this->MaxDepth = MaxDepth; }
  };

}








#endif
