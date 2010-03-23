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

  // This class speeds up search algorithm by hashing expressions
  // which are known to lead to a dead end. When such expressions are
  // recognized, the search algorith may safely skip them.
  class TransformationCache {
    // Inner class containing information for each hash table entry
    // We need to store two trees (one transforming into another) and the
    // depth used to take the conclusion that the expression can not be
    // transformed.
    struct CacheEntry {
      const Tree *LHS, *RHS;
      unsigned Depth;
    };
    // Each entry in the hash table leads to a colision list, capable
    // of holding many elements.
    typedef std::list<CacheEntry> ColisionList;
    // The hash table 
    ColisionList** HashTable;
    const unsigned HASHSIZE;

  public:
    // Constructor and destructor signatures
    TransformationCache();
    ~TransformationCache();
    // Public member functions
    inline void Add(const Tree* Exp, const Tree* Target, unsigned Depth);
    inline CacheEntry* LookUp(const Tree* Exp, const Tree* Target, 
			      unsigned Depth) const;   
  };

  // Main interface for search algorithms
  class Search {
    TransformationRules& RulesMgr;    
    InstrManager& InstructionsMgr;
    TransformationCache TransCache;

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
