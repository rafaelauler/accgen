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
#include <climits>

namespace backendgen {

  // SearchResult member functions

  SearchResult::SearchResult() {
    Instructions = new InstrList();
    Cost = INT_MAX;
  }

  SearchResult::~SearchResult() {
    delete Instructions;
  }

  // Search member functions

  // Constructor
  Search::Search(TransformationRules& RulesMgr, 
		 InstrManager& InstructionsMgr):
    RulesMgr(RulesMgr), InstructionsMgr(InstructionsMgr)
  { }

  // Auxiliary function returns true if two trees are directly
  // equivalent
  bool Compare (const Tree *E1, const Tree *E2) 
  {
    bool isOperator;
    // Notes: We match if E2 implements the same operation if
    // greater data size - this means we imply that E1 is the
    // wanted expression and E2 is the implementation proposal
    if ((isOperator = E1->isOperator()) == E2->isOperator() &&
	E1->getType() == E2->getType() &&
	E1->getSize() <= E2->getSize())
      {
	// Leaf?
	if (!isOperator) {
	  return true;
	}
	
	bool match = true;
	// Now we know we are handling with operators and may safely cast
	// and analyze its children
	const Operator *O1 = dynamic_cast<const Operator*>(E1),
	  *O2 = dynamic_cast<const Operator*>(E2);
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

  // Deallocated a decompose list
  inline void DeleteDecomposeList(std::list<Tree*>* List)
  {
    for (std::list<Tree*>::iterator I = List->begin(),
	   E = List->end(); I != E; ++I)
      {
	delete *I;
      }
    delete List;
  }

  // Extracts an expression's primary operator type
  inline unsigned PrimaryOperatorType (const Tree* Expression)
  {
    if (!Expression->isOperator())
      return 0;

    const Operator* O = dynamic_cast<const Operator*>(Expression);
    if (O->getType() == IfOp) {
      return PrimaryOperatorType((*O)[0]);
    } else if (O->getType() == AssignOp) {
      return PrimaryOperatorType((*O)[1]);
    }

    return O->getType();
  }

  inline bool Search::HasCloseSemantic(unsigned InstrPO, unsigned ExpPO)
  {
    // See if primary operators match naturally
    if (InstrPO == ExpPO)
      return true;

    // See if primary operators will match after a transformation
    for (RuleIterator I = RulesMgr.getBegin(), E = RulesMgr.getEnd();
	 I != E; ++I)
      {
	if (PrimaryOperatorType(I->LHS) == ExpPO &&
	    PrimaryOperatorType(I->RHS) == InstrPO)
	  return true;
	if (I->Equivalence && PrimaryOperatorType(I->RHS) == ExpPO &&
	    PrimaryOperatorType(I->LHS) == InstrPO)
	  return true;
      }
    return false;
  }

  // Try to apply transformation rules to bring
  // the expression semantics closer to something we might
  // know.
  SearchResult* Search::TransformExpression(const Tree* Expression)
  {    
    // Try to find an instruction "semantically close" of the 
    // expression. The instruction must match expression's top
    // operator, or there exists a transformation that makes this
    // matching feasible.
    const unsigned PO = PrimaryOperatorType(Expression);
    for (InstrIterator I = InstructionsMgr.getBegin(), 
	   E = InstructionsMgr.getEnd(); I != E; ++I) 
      {
	for (SemanticIterator I1 = (*I)->getBegin(), E1 = (*I)->getEnd();
	     I1 != E1; ++I1)
	  {
	    // Prune unworthy trials (heuristic)
	    if (!HasCloseSemantic(PrimaryOperatorType(*I1), PO))
	      continue;

	    // Try to apply transformations that bring Expression
	    // closer to this instruction (specifically, to this
	    // fragment of instruction), node by node.
	    for (RuleIterator I = RulesMgr.getBegin(), E = RulesMgr.getEnd();
		 I != E; ++I)
	      {
	       
	      }
	  }
      }

  }
  
  // This operator overload effectively starts the search
  SearchResult* Search::operator() (const Tree* Expression)
  {
    SearchResult* Result = new SearchResult();
    // First, see if this expression is directly computable by
    // an available instruction, or part of this instruction    
    for (InstrIterator I = InstructionsMgr.getBegin(), 
	   E = InstructionsMgr.getEnd(); I != E; ++I) 
      {
        // compare each semantic expression
	for (SemanticIterator I2 = (*I)->getBegin(), 
	       E2 = (*I)->getEnd(); I2 != E2; ++I2)
	  {
	    if (Compare(Expression, *I2) && 
		Result->Cost >= (*I)->getCost())
	      {
		Result->Cost = (*I)->getCost();
		Result->Instructions->clear();
		Result->Instructions->push_back(*I);
		break;
	      }
	  }
      }

    // If found something, return it
    if (Result->Cost != INT_MAX)
      return Result;

    // Look for decomposition rules and try to recursively search
    // for implementations for decomposed parts
    for (RuleIterator I = RulesMgr.getBegin(), E = RulesMgr.getEnd();
	 I != E; ++I)
      {
	if (!I->Decomposition && !I->Composition)
	  continue;

	std::list<Tree*>* DecomposeList = I->Decompose(Expression);
	if (DecomposeList == NULL)
	  continue;

	SearchResult* CandidateSolution = new SearchResult();

	for (std::list<Tree*>::iterator I1 = DecomposeList->begin(),
	       E1 = DecomposeList->end(); I1 != E1; ++I1)
	  {
	    SearchResult* ChildResult = (*this)(*I1);
	    // Failed to find an implementatin for this child?
	    if (ChildResult == NULL) {
	      delete CandidateSolution;
	      CandidateSolution = NULL;
	      break;
	    }

	    // Integrate results
	    CandidateSolution->Instructions->merge
	      (*(ChildResult->Instructions));
	    CandidateSolution->Cost += ChildResult->Cost;
	    delete ChildResult;
	  }
	
	DeleteDecomposeList(DecomposeList);
	// Check if it is a good solution
	if (CandidateSolution->Cost <= Result->Cost)
	  {
	    delete Result;
	    Result = CandidateSolution;
	  }
      }

    // If found something, return it
    if (Result->Cost != INT_MAX)
      return Result;

    // In the third step, apply transformation rules to bring
    // the expression semantics closer to something we might
    // know.

    

    // We can't find anything
    delete Result;
    return NULL;
  }

}
