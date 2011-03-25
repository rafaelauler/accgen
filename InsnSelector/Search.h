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

// TransCache is static, so it gets used between different searches.
// In order words, Search gets faster when it is used multiple times.
//#define STATIC_TRANSCACHE
// The above definition is currently controlled by Makefile
// This option is enabled by default.

// If parallel search is enabled, TemplateManager will fork multiple
// threads to search for more than one pattern at a time. To avoid
// data race conditions, STATIC_TRANSCACHE must be DISABLED.
//#define PARALLEL_SEARCH
// The above definition is currently controlled by Makefile
// Use "PARALLEL_SEARCH=1 make" to activate this and disable STATIC_TRANSCACHE.

using namespace backendgen::expression;

namespace backendgen {

  typedef std::list<std::string> NameListType;
  // This list should store operands for each instruction of the sequence
  // returned by SearchResult.
  typedef std::list<NameListType*> OperandsDefsType;
  // This list stores a mapping of virtual to real registers. This occur
  // when an expression matches with an instruction that uses real registers
  // in its semantics.
  typedef std::pair<std::string, std::string> RegPair;
  typedef std::list<RegPair> VirtualToRealMap;
  // This list stores a mapping of virtual registers to their register
  // classes, if this needs to be enforced in the search.
  typedef std::pair<std::string, const RegisterClass*> VCPair;
  typedef std::list<VCPair> VirtualClassesMap;
  // This list stores the rules applied to the searched expression for
  // debugging reasons.
  typedef std::list<unsigned> RulesAppliedList;
  // This pair stores the instruction and the specific semantic of that
  // instruction that matched our purposes in the search. (Remember
  // an instruction may have many semantic trees describing its behaviour).
  typedef std::list<std::pair<const Instruction*, SemanticIterator> >
  InstrList;
  typedef std::list<OperandTransformationList> OpTransLists;

  // This class encapsulates restrictions imposed to the search algorithm.
  // The virtual-to-real list says which tree leafs (operands) are already
  // bound. The virtual-classes says which tree leafs (operands) are
  // restricted to use a specific register class.
  class SearchRestrictions {
    VirtualToRealMap* VR;
    VirtualClassesMap* VC;
  public:
    SearchRestrictions(VirtualToRealMap* vr=0,
		       VirtualClassesMap* vc = 0);
    ~SearchRestrictions();
    VirtualToRealMap* getVR() {
      return VR;
    }
    VirtualClassesMap* getVC() {
      return VC;
    }

    void setVR(VirtualToRealMap* e);
    void setVC(VirtualClassesMap* e);
    bool AddToVRList(RegPair Element);
    void MergeVRList(const VirtualToRealMap* Source);
    bool AddToVCList(VCPair Element);
    void MergeVCList(const VirtualClassesMap* Source);
    void Merge(const SearchRestrictions* Source);    

    // Const member functions
    bool LookupVR(const std::string& Key, const std::string* Result) const;
    bool HasConflictingVRDefinitions(const VirtualToRealMap* B) const;
    bool LookupVC(const std::string& Key, const RegisterClass* Result) const;
    bool HasConflictingVCDefinitions(const VirtualClassesMap* B) const;
    bool HasConflictingDefinitions(const SearchRestrictions* B) const;
    const VirtualToRealMap* getVR() const;
    const VirtualClassesMap* getVC() const;
  };

  // Our functor (unary predicate) to decide if an element is member
  // of a VirtualToReal list
  class IsInVRFunctor {
    VirtualToRealMap* VR;
  public:
    IsInVRFunctor(VirtualToRealMap* list):
      VR(list) {}
    bool operator() (const Tree* A);
  };

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
    RulesAppliedList *RulesApplied;
    OpTransLists *OpTrans;
    SearchRestrictions* ST;
    SearchResult();
    ~SearchResult();
    void FilterAssignedNames();
    bool CheckVirtualToReal(const Tree *Exp) const;
    VirtualToRealMap::const_iterator VRLookupName(std::string S) const;
    void DumpResults(std::ostream& S) const;
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
    typedef std::list<CacheEntry>::iterator EntryIterator;
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
#ifdef STATIC_TRANSCACHE    
    static TransformationCache TransCache;
#else
    TransformationCache TransCache;
#endif

    unsigned MaxDepth;

    inline bool HasCloseSemantic(unsigned InstrPO, unsigned ExpPO);
    SearchResult* TransformExpression(const Tree* Expression,
				      const Tree* InsnSemantic, 
				      unsigned CurDepth,
				      const SearchRestrictions* ST);
    SearchResult* ApplyDecompositionRule(const Rule *R, const Tree* Expression,
					 const Tree* Goal, Tree *& MatchedGoal,
					 unsigned CurDepth, 
					 const SearchRestrictions* ST);
    bool TransformExpressionAux(const Tree* Transformed,
				const Tree* InsnSemantic, SearchResult* Result,
				unsigned CurDepth, 
				const SearchRestrictions *ST);
  public:
    Search(TransformationRules& RulesMgr, InstrManager& InstructionsMgr);
    SearchResult* operator() (const Tree* Expression, unsigned CurDepth,
			      const SearchRestrictions* ST);
    unsigned getMaxDepth() { return MaxDepth; }
    void setMaxDepth(unsigned MaxDepth) { this->MaxDepth = MaxDepth; }
  };

}








#endif
