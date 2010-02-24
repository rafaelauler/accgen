//===- Search.cpp - Implementation                         --*- C++ -*-----===//
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
  template <bool JustTopLevel>
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
	// If we don't wanna recursive comparison, that's enough
	if (JustTopLevel)
	  return true;

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
	  if (!Compare<false>((*O1)[I], (*O2)[I])) {
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

  // Apply a special kind of rule that disconnects the tree, and 
  // recursively tries to find implementations for each resulting tree.
  // If Goal is not NULL, then some tree in the decomposed set must
  // match it. For this tree, no implementation is searched, as it was
  // already matched to the goal.
  SearchResult* Search::ApplyDecompositionRule(const Rule *R,
					       const Tree* Expression,
					       const Tree* Goal,
					       Tree *& MatchedGoal)
  {
    if (!R->Decomposition && !R->Composition)
      return NULL;

    std::list<Tree*>* DecomposeList = R->Decompose(Expression);
    if (DecomposeList == NULL)
      return NULL;

    SearchResult* CandidateSolution = new SearchResult();

    for (std::list<Tree*>::iterator I = DecomposeList->begin(),
	   E = DecomposeList->end(); I != E; ++I)
      {
	// Check for goal
	if (Goal != NULL) {
	  if (Compare<true>(Goal, *I) == true &&
	      MatchedGoal == NULL) {
	    MatchedGoal = (*I)->clone();
	    continue;
	  }	    
	}
	SearchResult* ChildResult = (*this)(*I);
	// Failed to find an implementatin for this child?
	if (ChildResult == NULL) {
	  delete CandidateSolution;	  
	  DeleteDecomposeList(DecomposeList);
	  return NULL;	  
	}

	// Integrate results
	CandidateSolution->Instructions->merge(*(ChildResult->Instructions));
	CandidateSolution->Cost += ChildResult->Cost;
	delete ChildResult;
      }
	
    DeleteDecomposeList(DecomposeList);

    return CandidateSolution;
  }


  // Auxiliary function, tries to assert that a transformed tree (after
  // a rule has been applied to issue a transformation) is equal
  // to what we need (our goal). Recursively calls TransformExpression
  // to try to compare (and maybe transform) the children nodes.
  bool Search::TransformExpressionAux(Tree* Transformed, 
				      const Tree* InsnSemantic,
				      SearchResult* Result) {
    // First check the top level node
    if (Compare<true>(Transformed, InsnSemantic) == false) {
      return false;
    }
    
    // Transformation revealed that these expressions match directly
    if (Transformed->isOperand()) {
      Result->Cost = 0;
      return true;
    }
    
    const Operator* O = dynamic_cast<const Operator*>(Transformed);
    const Operator* OIns = dynamic_cast<const Operator*>(InsnSemantic);
    for (int I = 0, E = O->getArity(); I != E; ++I) 
      {
	SearchResult* SRChild = 
	  TransformExpression((*O)[I], (*OIns)[I]);
	
	// Failed
	if (SRChild->Cost == INT_MAX) {
	  delete SRChild;
	  return false;
	}
	
	// This child matches without decomposition
	if (SRChild->Cost == 0) {
	  delete SRChild;
	  continue;
	}
	
	// This child matched, but applied a decomposition rule and
	// we need to integrate its results
	Result->Cost += SRChild->Cost;
	Result->Instructions->merge(*(SRChild->Instructions));
	delete SRChild;
      }
    return true;
  }

  // Try to apply transformation rules to bring
  // the expression semantics closer to something we might
  // know (the Instruction Semantic)
  SearchResult* Search::TransformExpression(const Tree* Expression,
					    const Tree* InsnSemantic)
  {   
    SearchResult* Result = new SearchResult();

    const unsigned PO = PrimaryOperatorType(Expression);

    // Prune unworthy trials (heuristic)
    if (!HasCloseSemantic(PrimaryOperatorType(InsnSemantic), PO))
      return Result;

    // See if we already have a match
    if (Compare<false>(Expression, InsnSemantic) == true) {
      Result->Cost = 0;
      return Result;
    }      
    
    // Try to apply transformations that bring Expression
    // closer to this instruction (specifically, to this
    // fragment of instruction), node by node.
    for (RuleIterator I = RulesMgr.getBegin(), E = RulesMgr.getEnd();
	 I != E; ++I)
      {
	bool Forward = true;
	// See if makes sense applying this rule
	if (!I->ForwardMatch(Expression) || 
	    PrimaryOperatorType(I->RHS) != PrimaryOperatorType(InsnSemantic)) 
	  {
	    if (!I->BackwardMatch(Expression) || 
		PrimaryOperatorType(I->LHS) != 
		PrimaryOperatorType(InsnSemantic))
	      continue;
	    Forward = false;
	  }

	// Case analysis 1: Suppose this rule does not decompose the
	// tree
	if ((!Forward || !I->Decomposition) &&
	    (Forward || !I->Composition)) {
	
	  // Apply
	  Tree* Transformed = Forward? I->ForwardApply(Expression) :
	    I->BackwardApply(Expression);
	
	  // Calls auxiliary to confirm that the transformed expression
	  // equals our goal
	  // This may involve recursive calls to this function (to transform
	  // and adapt the children nodes).
	  if (TransformExpressionAux(Transformed, InsnSemantic, Result) == true)
	    {
	      delete Transformed;
	      return Result;
	    }
	  delete Transformed;
	  continue;
	} 

	// Case analysis 2: This rule decomposes the tree
	Tree* Transformed = NULL;
	SearchResult* ChildResult = 
	  ApplyDecompositionRule(&*I, Expression, InsnSemantic, Transformed);
	
	//Failed
	if (ChildResult == false) {
	  if (Transformed != NULL)
	    delete Transformed;
	  continue;
	}
	
	// Now we decomposed and have a implementation of the remaining
	// trees. We just need to assert that the transformed tree
	// is really what we want.
	// Calls auxiliary to confirm that the transformed expression
	// equals our goal
	// This may involve recursive calls to this function (to transform
	// and adapt the children nodes).
	if (TransformExpressionAux(Transformed, InsnSemantic, Result) == true)
	  {
	    delete Transformed;
	    return Result;
	  }
	delete Transformed;			    			 
      } // end for(RuleIterator)

    // We tried but could not find anything
    return Result;
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
	    if (Compare<false>(Expression, *I2) && 
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
	Tree* dummy;
	SearchResult* CandidateSolution = ApplyDecompositionRule(&*I, 
								 Expression,
								 NULL, dummy);

	if (CandidateSolution == NULL)
	  continue;
	
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
    // Try to find an instruction "semantically close" of the 
    // expression. The instruction must match expression's top
    // operator, or there exists a transformation that makes this
    // matching feasible.
    for (InstrIterator I = InstructionsMgr.getBegin(), 
	   E = InstructionsMgr.getEnd(); I != E; ++I) 
      {
	for (SemanticIterator I1 = (*I)->getBegin(), E1 = (*I)->getEnd();
	     I1 != E1; ++I1)
	  {
	    SearchResult* CandidateSolution = 
	      TransformExpression(Expression, *I1);

	    // Failed
	    if (CandidateSolution->Cost == INT_MAX) {
	      delete CandidateSolution;
	      continue;
	    }	     

	    // Check if it is a good solution
	    if (CandidateSolution->Cost <= Result->Cost)
	      {
		delete Result;
		Result = CandidateSolution;
	      }	
	  }
      }

    // If found something, return it
    if (Result->Cost != INT_MAX)
      return Result;    

    // We can't find anything
    delete Result;
    return NULL;
  }

}
