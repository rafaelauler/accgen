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
#include <cassert>

//#define DEBUG
//#define USETRANSCACHE

namespace backendgen {

  // Debug methods

#ifdef DEBUG
  inline void DbgPrint(const std::string &text) {
    std::cerr << text;
  } 
  inline void DbgIndent(unsigned CurDepth) {
    for (unsigned I = 0; I < CurDepth; ++I)
      std::cerr << " ";
  }
#define Dbg(x) x
#else
  inline void DbgPrint(const std::string &text) {}
  inline void DbgIndent(unsigned CurDepth) {}
#define Dbg(x)
#endif


  // Auxiliaries EqualTypes and EqualNodeTypes are used in the
  // prune heuristic to compare node types

  // A special comparison that considers if the node type has value 0,
  // in which cases this type matches all
  inline bool EqualTypes(const unsigned T1, const unsigned T2)
  {
    return (T1 == T2 || T1 == 0 || T2 == 0);
  }

  // A special comparison that considers if the node type or size has value 0,
  // in which cases this type matches all
  template<class T>
  inline bool EqualNodeTypes(const T &T1, const T &T2) 
  {
    return ((T1.Type == T2.Type || T1.Type == 0 || T2.Type == 0) &&
	    (T1.Size <= T2.Size || T1.Size == 0 || T2.Size == 0));
  }

  // This template is extensively used in the Search algorithm to 
  // conclude if two expressions are equal. If the template is instantiated
  // with JustTopLevel=true, the function doesn't descend into childrens, but
  // just compare the top level node.
  template <bool JustTopLevel>
  bool Compare (const Tree *E1, const Tree *E2)
  {
    bool isOperator;
    // Notes: We match if E2 implements the same operation with
    // greater data size - this means we imply that E1 is the
    // wanted expression and E2 is the implementation proposal
    if ((isOperator = E1->isOperator()) == E2->isOperator() &&
	E1->getType() == E2->getType() &&
	E1->getSize() <= E2->getSize())
      {
	// Leaf?
	if (!isOperator) {
	  const Constant* C1 = dynamic_cast<const Constant*>(E1);
	  // Depending on the operand type, we need to make further checkings
	  // Constants must be equal
	  if (C1 != NULL) {
	    const Constant* C2 = dynamic_cast<const Constant*>(E2);
	    assert (C2 != NULL && "Nodes must agree in type at this stage.");
	    if (C1->getConstValue() != C2->getConstValue())
	      return false;
	  }
	  return true;
	}	

	if (JustTopLevel)
	  return true;

	// Now we know we are handling with operators
	// See if this is a guarded assignment
	const AssignOperator *AO1 = dynamic_cast<const AssignOperator*>(E1),
	  *AO2 = dynamic_cast<const AssignOperator*>(E2);
	assert ((AO1 == NULL) == (AO2 == NULL) &&
		"Nodes must agree with type at this stage");
	if (AO1 != NULL) {
	  // If they don't agree with having a predicate, they're different
	  // AO->getPredicate() == NULL xor AOIns->getPredicate() == NULL
	  if ((AO1->getPredicate() != NULL && AO2->getPredicate() == NULL) ||
	      (AO1->getPredicate() == NULL && AO2->getPredicate() != NULL) )
	    return false;
	  if (AO1->getPredicate()) {
	    // Check if the comparators for the guarded assignments are equal
	    if (AO1->getPredicate()->getComparator() != 
		AO2->getPredicate()->getComparator())
	      return false;
	    // Now check if the expressions being compared are equal
	    if (!Compare<false>(AO1->getPredicate()->getLHS(), 
				AO2->getPredicate()->getLHS()))
	      return false;
	    if (!Compare<false>(AO1->getPredicate()->getRHS(), 
				AO2->getPredicate()->getRHS()))
	      return false;
	  }
	}

	bool match = true;	
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

  
  // SearchResult member functions

  SearchResult::SearchResult() {
    Instructions = new InstrList();
    Cost = INT_MAX;
    OperandsDefs = new OperandsDefsType();
  }

  SearchResult::~SearchResult() {
    delete Instructions;
    for (OperandsDefsType::iterator I = OperandsDefs->begin(),
	   E = OperandsDefs->end(); I != E; ++I)
      {
	delete *I;
      }
    delete OperandsDefs;
  }

  // TransformationCache member functions
  TransformationCache::TransformationCache() : HASHSIZE(1024) {
    HashTable = new ColisionList* [HASHSIZE];
    assert (HashTable != NULL && "MemAlloc fail");
    for (unsigned I = 0, E = HASHSIZE; I != E; ++I) {
      HashTable[I] = NULL;
    }
  }
  
  TransformationCache::~TransformationCache() {
    for (unsigned I = 0, E = HASHSIZE; I != E; ++I) {
      if (HashTable[I] != NULL) {
	delete HashTable[I];
	HashTable[I] = NULL;
      }
    }
    delete [] HashTable;
  }

  inline void TransformationCache::Add(const Tree* Exp, const Tree* Target,
				       unsigned Depth) {
    CacheEntry NewEntry = {Exp->clone(), Target->clone(), Depth};
    unsigned Hash = Exp->getHash(Target->getHash()) % HASHSIZE;
    ColisionList *ColList = HashTable[Hash];    
    if (ColList == NULL) {
      ColList = HashTable[Hash] = new ColisionList();    
    }
    ColList->push_back(NewEntry);
    return;
  }

  inline TransformationCache::CacheEntry* TransformationCache::LookUp
  (const Tree* Exp, const Tree* Target, unsigned Depth) const {
    unsigned Hash = Exp->getHash(Target->getHash()) % HASHSIZE;
    ColisionList *ColList = HashTable[Hash];
    // Miss
    if (ColList == NULL) {
      return NULL;
    }
    for (ColisionList::iterator I = ColList->begin(), E = ColList->end();
	 I != E; ++I) {
      CacheEntry &Entry = *I;
      if (Compare<false>(Exp, Entry.LHS) && 
	  Compare<false>(Target, Entry.RHS) &&
	  Depth <= Entry.Depth)
	return &Entry;
    }
    return NULL;
  }

  // Search member functions

  // Constructor
  Search::Search(TransformationRules& RulesMgr, 
		 InstrManager& InstructionsMgr):
    RulesMgr(RulesMgr), InstructionsMgr(InstructionsMgr)
  { 
    MaxDepth = 10; // default search depth, if none specified this will be
                   // used
  }

  // This auxiliary function searches for leafs in an expression and
  // gets their names and put they in a list.
  // With this information, we know what names to use as operands
  // of instructions.
  NameListType* ExtractLeafsNames(const Tree* Exp) {	
    // Sanity check
    if (Exp == NULL)
      return NULL;
    
    NameListType* Result = new NameListType();

    // A leaf
    if (Exp->isOperand()) {
      // Constants don't have names, ignore them
      if (dynamic_cast<const Constant*>(Exp)) 
	return Result;
      // Immediates don't have names, ignore them
      if (dynamic_cast<const ImmediateOperand*>(Exp))
	return Result;
      // We should not expect fragments to be present here
      assert(dynamic_cast<const FragOperand*>(Exp) == NULL &&
	     "Unexpected node type.");
      const Operand* O = dynamic_cast<const Operand*>(Exp);
      Result->push_back(O->getOperandName());
      return Result;
    }

    // An operator
    const Operator* O = dynamic_cast<const Operator*>(Exp);
    const AssignOperator* AO = dynamic_cast<const AssignOperator*>(Exp);
      if (AO != NULL && AO->getPredicate() != NULL) {
	NameListType* ChildResult = ExtractLeafsNames
	  (AO->getPredicate()->getLHS());
	assert (ChildResult != NULL && "Must return a valid pointer");
	Result->merge(*ChildResult);
	delete ChildResult;
	ChildResult = ExtractLeafsNames(AO->getPredicate()->getRHS());
	assert (ChildResult != NULL && "Must return a valid pointer");
	Result->merge(*ChildResult);
	delete ChildResult;	       
      }
    for (int I = 0, E = O->getArity(); I != E; ++I) 
      {
	NameListType* ChildResult = ExtractLeafsNames((*O)[I]);
	assert (ChildResult != NULL && "Must return a valid pointer");
	Result->merge(*ChildResult);
	delete ChildResult;
      }
    return Result;
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

  // Extracts a predicate primary operator type
  inline unsigned PrimaryOperatorType(const Predicate* Pred) {
    return Pred->getComparator() + 5000;
  }

  // Extracts an expression's primary operator type
  inline unsigned PrimaryOperatorType (const Tree* Expression)
  {
    if (!Expression->isOperator())
      return 0;

    const Operator* O = dynamic_cast<const Operator*>(Expression);
    if (O->getType() == AssignOp) {
      const AssignOperator* AO = dynamic_cast<const AssignOperator*>
      	(Expression);      
      assert(AO != NULL && "AssignOp not in a AssignOperator type");
      // Ignores the predicate currently, for heuristic reasons
      //if (AO->getPredicate() != NULL)
      //	return PrimaryOperatorType(AO->getPredicate());
      return PrimaryOperatorType((*O)[1]);
    }    

    return O->getType();
  }

  inline bool Search::HasCloseSemantic(unsigned InstrPO, unsigned ExpPO)
  {
    // See if primary operators match naturally
    if (EqualTypes(InstrPO, ExpPO))
      return true;

    // See if primary operators will match after a transformation
    for (RuleIterator I = RulesMgr.getBegin(), E = RulesMgr.getEnd();
	 I != E; ++I)
      {
	if (EqualTypes(PrimaryOperatorType(I->LHS), ExpPO) &&
	    EqualTypes(PrimaryOperatorType(I->RHS), InstrPO))
	  return true;
	if (I->Equivalence && EqualTypes(PrimaryOperatorType(I->RHS), ExpPO)
	    && EqualTypes(PrimaryOperatorType(I->LHS), InstrPO))
	  return true;
      }
    return false;
  }

  // Auxiliary function used to integrate the results of a recursive
  // call to Search, which itself returns a particular SearchResult,
  // with the current SearchResult being held by a caller function.
  inline
  void MergeSearchResults(SearchResult* Destination, SearchResult* Source)
  {
    // Assure the source is valid
    if (Source == NULL)
      return;

    // If we are merging results, we need at least _have_ a result.
    assert (Source->Cost != INT_MAX);

    // Check OperandsDefs. If a current definition list is under construction,
    // it will haveno instruction associated and therefore the list of
    // OperandsDefs will be greater than the list of Instructions. If both
    // destination and source have a definition list under constructions,
    // we need to merge both.
    if (Destination->OperandsDefs->size() > Destination->Instructions->size()
	&& Source->OperandsDefs->size() > Source->Instructions->size())
      {
	// The list under construction is necessarily the last one
	Destination->OperandsDefs->back()
	  ->splice(Destination->OperandsDefs->back()->end(), 
		   *(Source->OperandsDefs->back()));
	// Already transfered its contents, now delete it.
	delete Source->OperandsDefs->back();
	Source->OperandsDefs->pop_back();
      }
    // In the other case, exactly one has a list under construction, or none.
    // We don't bother with this, as we simply transfer all remaining
    // definitions from source to destination.
    Destination->OperandsDefs->splice(Destination->OperandsDefs->begin(),
				      *(Source->OperandsDefs));
    // Now tranfer discovered instructions in Source
    Destination->Instructions->splice(Destination->Instructions->begin(),
				      *(Source->Instructions));
    // Now merge cost

    // If Destination doesn't have a solution yet, set its cost to zero
    // so as to properly sum with Source's cost;
    if (Destination->Cost == INT_MAX) 
      Destination->Cost = 0;
    Destination->Cost += Source->Cost;

    //Merge successful
    return;
  }

  // This auxiliary function is used by search routines whenever an
  // operand is matched and we need to store its name (operand definition)
  // in the SearchResult record. As we not know yet the instruction
  // to whom this operand pertains, we may need to allocate an orphan
  // operandsdefs list (without an associated instruction). To be easily
  // located, this orphan list is necessarily the last one in the list
  // of operands definitions. The list may be already allocated, so we also
  // check for this case in this function.
  void UpdateCurrentOperandDefinition(SearchResult* Result,
				      NameListType* Definitions) {
    // The orphan list is already allocated, so we just merge these
    // new definitions in it
    if (Result->OperandsDefs->size() > Result->Instructions->size()) 
      {
	Result->OperandsDefs->back()
	  ->splice(Result->OperandsDefs->back()->end(), *Definitions);
	delete Definitions;
	return;
      }

    // Otherwise, we need to allocate the orphan list. In fact, we directly
    // use "Definitions" as the new member list.
    Result->OperandsDefs->push_back(Definitions);
    return;  
  }

  // Apply a special kind of rule that disconnects the tree, and 
  // recursively tries to find implementations for each resulting tree.
  // If Goal is not NULL, then some tree in the decomposed set must
  // match it. For this tree, no implementation is searched, as it was
  // already matched to the goal.
  SearchResult* Search::ApplyDecompositionRule(const Rule *R,
					       const Tree* Expression,
					       const Tree* Goal,
					       Tree *& MatchedGoal,
					       unsigned CurDepth)
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
	SearchResult* ChildResult = (*this)(*I, CurDepth + 1);
	// Failed to find an implementatin for this child?
	if (ChildResult == NULL || ChildResult->Cost == INT_MAX) {
	  delete CandidateSolution;	  
	  if (ChildResult) delete ChildResult;
	  DeleteDecomposeList(DecomposeList);
	  return NULL;	  
	}

	// Integrate results
	MergeSearchResults(CandidateSolution, ChildResult);
	delete ChildResult;
      }
	
    DeleteDecomposeList(DecomposeList);

    return CandidateSolution;
  }


  // Auxiliary function, tries to assert that a transformed tree (after
  // a rule has been applied to issue a transformation) is equal
  // to what we need (our goal). Recursively calls TransformExpression
  // to try to compare (and maybe transform) the children nodes.
  bool Search::TransformExpressionAux(const Tree* Transformed, 
				      const Tree* InsnSemantic,
				      SearchResult* Result,
				      unsigned CurDepth) {    
    // First check the top level node
    if (Compare<true>(Transformed, InsnSemantic) == false) {
      return false;
    }
    
    // Transformation revealed that these expressions match directly
    if (Transformed->isOperand()) {
      Result->Cost = 0;
      UpdateCurrentOperandDefinition(Result, ExtractLeafsNames(Transformed));
      return true;
    }
    // Now check for guarded assignments before delving into each children
    // of this operator
    SearchResult* TempResults = new SearchResult();

    const AssignOperator* AO = dynamic_cast<const AssignOperator*>(Transformed);
    const AssignOperator* AOIns = 
      dynamic_cast<const AssignOperator*>(InsnSemantic);
    if (AO != NULL) {
      // If they don't agree with having a predicate, they're different
      // AO->getPredicate() == NULL xor AOIns->getPredicate() == NULL
      if ((AO->getPredicate() != NULL && AOIns->getPredicate() == NULL) ||
	  (AO->getPredicate() == NULL && AOIns->getPredicate() != NULL) )
	return false;
      //If both have a predicate
      if (AO->getPredicate() != NULL) {
	// Check the comparator
	if (AO->getPredicate()->getComparator() != AOIns->getPredicate()
	    ->getComparator())
	  return false;
	// Check expressions being compared (involve recursive calls)
	SearchResult* SR_LHS = 
	  TransformExpression(AO->getPredicate()->getLHS(), 
			      AOIns->getPredicate()->getLHS(), CurDepth+1);
	// Failed to prove LHSs equal
	if (SR_LHS->Cost == INT_MAX) {
	  delete TempResults;
	  delete SR_LHS;
	  return false;
	}
	MergeSearchResults(TempResults, SR_LHS);
	delete SR_LHS;
	SearchResult* SR_RHS =
	  TransformExpression(AO->getPredicate()->getRHS(), 
			      AOIns->getPredicate()->getRHS(), CurDepth+1);
	// Failed to prove RHSs equal
	if (SR_RHS->Cost == INT_MAX) {
	  delete TempResults;
	  delete SR_RHS;
	  return false;
	}
	MergeSearchResults(TempResults, SR_RHS);
	delete SR_RHS;
      }// end if both have a predicate
    }// end if they are assignop

    // Now for every operator, prove their children equal
    const Operator* O = dynamic_cast<const Operator*>(Transformed);
    const Operator* OIns = dynamic_cast<const Operator*>(InsnSemantic);
    for (int I = 0, E = O->getArity(); I != E; ++I) 
      {
	SearchResult* SRChild = 
	  TransformExpression((*O)[I], (*OIns)[I], CurDepth+1);
	
	// Failed
	if (SRChild->Cost == INT_MAX) {
	  DbgIndent(CurDepth);
	  DbgPrint("Recursive call failed\n");
	  delete TempResults;
	  delete SRChild;
	  return false;
	}
	
	DbgIndent(CurDepth);
	DbgPrint("Recursive call was successful\n");	
	// Match for child was successful, so we need to integrate its results
	MergeSearchResults(TempResults, SRChild);
	delete SRChild;
      }
    // Everything went ok, integrate TempResults with Results    
    MergeSearchResults(Result, TempResults);
    delete TempResults;
    return true;
  }

  // Try to apply transformation rules to bring
  // the expression semantics closer to something we might
  // know (the Instruction Semantic)
  SearchResult* Search::TransformExpression(const Tree* Expression,
					    const Tree* InsnSemantic,
					    unsigned CurDepth)
  {   
    DbgIndent(CurDepth);
    DbgPrint("Entering TransformExpression\n        Expression: ");
    Dbg(Expression->print(std::cerr));
    DbgPrint("\n        Goal:");
    Dbg(InsnSemantic->print(std::cerr));
    DbgPrint("\n\n");

    SearchResult* Result = new SearchResult();

    // Cancel this trial if it has exceeded maximum recursive depth allowed
    if (CurDepth == MaxDepth) {
      DbgIndent(CurDepth);
      DbgPrint("Maximum recursive depth reached.\n");
      return Result;
    }

#ifdef USETRANSCACHE
    // Check Transformation Cache to see if transforming Expression
    // into InsnSemantic is a dead end
    if (TransCache.LookUp(Expression, InsnSemantic, MaxDepth-CurDepth)) {
      DbgIndent(CurDepth);
      DbgPrint("Cache informs us there is no such transformation.\n");
      return Result;
    }
#endif
    
    const unsigned PO = PrimaryOperatorType(Expression);

    // Prune unworthy trials (heuristic)
    if (!HasCloseSemantic(PrimaryOperatorType(InsnSemantic), PO)) {
      DbgIndent(CurDepth);
      DbgPrint("CloseSemantic heuristic pruned this trial.\n");
      return Result;
    }

    // See if we already have a match
    if (Compare<false>(Expression, InsnSemantic) == true) {
      DbgIndent(CurDepth);
      DbgPrint("Already matches!\n");
      Result->Cost = 0;
      UpdateCurrentOperandDefinition(Result, ExtractLeafsNames(Expression));
      return Result;
    }      

    DbgIndent(CurDepth);
    DbgPrint("Trying to transform children without applying any rule");   
    DbgPrint(" at the top level node...\n");

    // See if we obtain success without applying a transformation
    // at this level
    if (TransformExpressionAux(Expression, InsnSemantic, Result, CurDepth) 
	== true)      	
	return Result;
      
    DbgIndent(CurDepth);
    DbgPrint("Fail. Trying transformation rules...\n");
    
    // Try to apply transformations that bring Expression
    // closer to this instruction (specifically, to this
    // fragment of instruction), node by node.
    for (RuleIterator I = RulesMgr.getBegin(), E = RulesMgr.getEnd();
	 I != E; ++I)
      {
	bool Forward = true;
	// See if makes sense applying this rule
	if (!I->ForwardMatch(Expression) || 
	    !EqualTypes(PrimaryOperatorType(I->RHS),
			PrimaryOperatorType(InsnSemantic))) 
	  {
	    if (!I->BackwardMatch(Expression) || 
		!EqualTypes(PrimaryOperatorType(I->LHS), 
			    PrimaryOperatorType(InsnSemantic)))
	      continue;
	    Forward = false;
	  }

	// Case analysis 1: Suppose this rule does not decompose the
	// tree
	if ((!Forward || !I->Decomposition) &&
	    (Forward || !I->Composition)) {       

	  DbgIndent(CurDepth);
	  DbgPrint("Applying non-decomposing rule:\n  ");
	  DbgIndent(CurDepth);
	  Dbg(I->Print(std::cerr));
	  DbgPrint("\n");

	  // Apply
	  Tree* Transformed = Forward? I->ForwardApply(Expression) :
	    I->BackwardApply(Expression);
	
	  // Calls auxiliary to confirm that the transformed expression
	  // equals our goal
	  // This may involve recursive calls to this function (to transform
	  // and adapt the children nodes).
	  if (TransformExpressionAux(Transformed, InsnSemantic, Result, 
				     CurDepth) == true)
	    {
	      delete Transformed;
	      return Result;
	    }
	  delete Transformed;
	  continue;
	} 

	// Case analysis 2: This rule decomposes the tree
	DbgIndent(CurDepth);
	DbgPrint("Applying decomposing rule\n");
	DbgIndent(CurDepth);
	Dbg(I->Print(std::cerr));
	DbgPrint("\n");

	Tree* Transformed = NULL;
	SearchResult* ChildResult = 
	  ApplyDecompositionRule(&*I, Expression, InsnSemantic, Transformed,
				 CurDepth);
	
	//Failed
	if (ChildResult == NULL || ChildResult->Cost == INT_MAX) {
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
	if (TransformExpressionAux(Transformed, InsnSemantic, Result,
				   CurDepth) == true)
	  {
	    DbgIndent(CurDepth);
	    DbgPrint("Decomposition was successful\n");
	    MergeSearchResults(Result, ChildResult);
	    delete ChildResult;
	    delete Transformed;
	    return Result;
	  }
	DbgIndent(CurDepth);
	DbgPrint("Failed to match decomposed tree with our requisites\n");
	delete ChildResult;
	delete Transformed;			    			 
      } // end for(RuleIterator)

    DbgIndent(CurDepth);
    DbgPrint("Fail to prove both expressions are equivalent.\n");

#ifdef USETRANSCACHE
    TransCache.Add(Expression, InsnSemantic, MaxDepth-CurDepth);
#endif

    // We tried but could not find anything
    return Result;
  }
  
  // This operator overload effectively starts the search
  SearchResult* Search::operator() (const Tree* Expression, unsigned CurDepth)
  {
    DbgIndent(CurDepth);
    DbgPrint("Search started on ");
    Dbg(Expression->print(std::cerr));
    DbgPrint("\n");

    SearchResult* Result = new SearchResult();

    // Cancel this trial if it has exceeded maximum recursive depth allowed
    if (CurDepth == MaxDepth) {
      DbgIndent(CurDepth);
      DbgPrint("Maximum recursive depth reached.\n");
      return Result;
    }

    DbgIndent(CurDepth);
    DbgPrint("Trying direct match\n");
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
		delete Result;
		Result = new SearchResult();
		Result->Cost = (*I)->getCost();
		Result->Instructions->push_back(std::make_pair(*I,I2));
		UpdateCurrentOperandDefinition(Result, 
					       ExtractLeafsNames(Expression));
		break;
	      }
	  }
      }

    // If found something, return it
    if (Result->Cost != INT_MAX) {
      DbgIndent(CurDepth);
      DbgPrint("Direct match successful\n");
      return Result;
    }

#if 0
    DbgIndent(CurDepth);
    DbgPrint("Trying decomposition rules\n");

    // Look for decomposition rules and try to recursively search
    // for implementations for decomposed parts
    for (RuleIterator I = RulesMgr.getBegin(), E = RulesMgr.getEnd();
	 I != E; ++I)
      {
	Tree* dummy;
	SearchResult* CandidateSolution = ApplyDecompositionRule(&*I, 
								 Expression,
								 NULL, dummy,
								 CurDepth);

	if (CandidateSolution == NULL)
	  continue;
	
	// Check if it is a good solution
	if (CandidateSolution->Cost <= Result->Cost)
	  {
	    delete Result;
	    Result = CandidateSolution;
	  }
	else
	  delete CandidateSolution;
      }

    // If found something, return it
    if (Result->Cost != INT_MAX)
      return Result;
#endif

    DbgIndent(CurDepth);
    DbgPrint("Trying transformations\n");

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
	      TransformExpression(Expression, *I1, CurDepth);

	    // Failed
	    if (CandidateSolution->Cost == INT_MAX) {
	      delete CandidateSolution;
	      continue;
	    }	     

	    // Integrate our instruction
	    CandidateSolution->Cost += (*I)->getCost();
	    CandidateSolution->Instructions->push_back(std::make_pair(*I,I1));
	    // OperandsDefs for this insn should already be in the list
	    // thanks to TransformExpression that decoded its operands
	    // from the expression

	    // Check if it is a good solution
	    if (CandidateSolution->Cost <= Result->Cost)
	      {
		delete Result;
		Result = CandidateSolution;		
	      }	
	    else
	      delete CandidateSolution;
	  }
      }

    // If found something, return it
    if (Result->Cost != INT_MAX)
      return Result;    

    // We can't find anything
    return Result;
  }

}
