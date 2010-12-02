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
#include <sstream>
#include <cstdlib>
#include <cassert>

namespace backendgen {

  using namespace backendgen::expression;

  // Static number used when generating operands names. Start at 200.
  unsigned Rule::OpNum = 200;

  // Traverse tree looking for a specific operator type
  bool FindOperator(Tree* T, unsigned OpType) {
    if (T->isOperator()) {
      if (T->getType() == OpType)
	return true;
      Operator *O = dynamic_cast<Operator*>(T);
      for (int I = 0, E = O->getArity(); I != E; ++I)
	{
	  if (FindOperator((*O)[I], OpType))
	    return true;
	}
    }

    return false;
  } 

  // Rule member functions

  // Constructor
  Rule::Rule(Tree* LHS, Tree* RHS, bool Equivalence, unsigned ID):
    LHS(LHS),RHS(RHS),Equivalence(Equivalence),RuleID(ID),OpTransList()
  {
    Composition = FindOperator(LHS, DecompOp);
    Decomposition = FindOperator(RHS, DecompOp);
  }
  
  Rule::Rule(Tree* LHS, Tree* RHS, bool Equivalence, unsigned ID,
	     OperandTransformationList &OList):
    LHS(LHS),RHS(RHS),Equivalence(Equivalence),RuleID(ID),OpTransList(OList)
  {
    Composition = FindOperator(LHS, DecompOp);
    Decomposition = FindOperator(RHS, DecompOp);
  }

  typedef std::pair<std::string,const Tree*> AnnotatedTree;
  typedef std::list<AnnotatedTree> AnnotatedTreeList;
  
  // This auxiliary function is similar to Search.cpp:Compare(),
  // except that rules comparison are stricter, since we need
  // to match exact data types, unless explictly said by the
  // user (using datatype keyword "any").

  // Also, it tries to return E - R (the remaining trees obtained
  // when the matched nodes are removed), so the expression can
  // be properly rewritten. This is returned as a list of nodes.
  // If this is list is NULL, the matching failed.
  template <bool JustCompare> 
  AnnotatedTreeList* MatchExpByRule(const Tree* R, const Tree* E) 
  {
    bool isOperator;
    // Here we match operator by operator, operand by operand
    //NOTE: MemRef is a special case not affected by wildcard rules
    if ((isOperator = R->isOperator()) == E->isOperator() &&
	((R->getType() == E->getType() &&
	(R->getSize() == E->getSize() || R->getSize() == 0 || 
	 E->getSize() == 0)) ||
	 (R->getType() == 0 && (E->isOperator() == false 
	                     || E->getType() != MemRefOp))) )
      {
	AnnotatedTreeList* Result = NULL;
	if (!JustCompare) Result = new AnnotatedTreeList();

	// Leaf? Cast to operand
	if (!isOperator) {	
	  // If not wildcard, then it must be a perfect match.
	  if (R->getType() != 0) {
	    
	    const Constant* C1 = dynamic_cast<const Constant*>(R);
	    const Constant* C2 = dynamic_cast<const Constant*>(E);
	    if ((C1 == NULL && C2 != NULL) || (C1 != NULL && C2 == NULL)) {
	      if (!JustCompare) delete Result;
	      return NULL;
	    }
	    // Depending on the operand type, we need to make further checkings
	    // Constants must be equal
	    if (C1 != NULL) {	    	    	    
	      if (C1->getConstValue() != C2->getConstValue()) {
		if (!JustCompare) delete Result;
		return NULL;
	      }
	    }
	    
	    // Immediate handling
	    const ImmediateOperand* I1 = 
	      dynamic_cast<const ImmediateOperand*>(R);
	    const ImmediateOperand* I2 = 
	      dynamic_cast<const ImmediateOperand*>(E);
	    if ((I1 == NULL && I2 != NULL) || (I1 != NULL && I2 == NULL)) {
	      if (!JustCompare) delete Result;
	      return NULL;
	    }
	  }

	  // References to specific registers must be equal
	  const Operand * O1 = dynamic_cast<const Operand*>(R);	 
#if 0
	  const Operand * O2 = dynamic_cast<const Operand*>(E);
	  assert (O1 != NULL && O2 != NULL && "Nodes must be operands");
	  if ((O2->isSpecificReference() && !O1->isSpecificReference()) ||
	      (!O2->isSpecificReference() && O1->isSpecificReference())) {
	    if (!JustCompare) delete Result;
	    return NULL;
	  }
	  // If both are specific references, they must match ref. names
	  if (O2->isSpecificReference() && 
	      O1->getOperandName() != O2->getOperandName()) {
	    if (!JustCompare) delete Result;
	    return NULL;
	  }
#endif
	  // Both operands agree
	  if (JustCompare) {	    
	    return reinterpret_cast<AnnotatedTreeList*>(1);
	  }
	  AnnotatedTree AT(O1->getOperandName(), E);
	  Result->push_back(AT);
	  return Result;
	}
	
	// Now we know we are handling with operators
	// See if this is a guarded assignment
	const AssignOperator *AO1 = dynamic_cast<const AssignOperator*>(R),
	  *AO2 = dynamic_cast<const AssignOperator*>(E);
	assert ((AO1 == NULL) == (AO2 == NULL) &&
		"Nodes must agree with type at this stage");
	if (AO1 != NULL) {
	  // If they don't agree with having a predicate, they're different
	  // AO->getPredicate() == NULL xor AOIns->getPredicate() == NULL
	  if ((AO1->getPredicate() != NULL && AO2->getPredicate() == NULL) ||
	      (AO1->getPredicate() == NULL && AO2->getPredicate() != NULL) ) {
	    if (!JustCompare) delete Result;
	    return NULL;
	  }
	  if (AO1->getPredicate()) {
	    // Check if the comparators for the guarded assignments are equal
	    if (AO1->getPredicate()->getComparator() != 
		AO2->getPredicate()->getComparator()) {
	      if (!JustCompare) delete Result;
	      return NULL;
	    }
	    // Now check if the expressions being compared are equal
	    AnnotatedTreeList *ChildResult =
	      MatchExpByRule<JustCompare>(AO1->getPredicate()->getLHS(), 
					  AO2->getPredicate()->getLHS());
	    if (JustCompare) {
	      if (ChildResult == 0)
	        return 0;	      		
	    } else {
	      if (ChildResult == NULL) {
		delete Result;
		return NULL;
	      }
	      Result->merge(*ChildResult);
	      delete ChildResult;
	    }
	    ChildResult = MatchExpByRule<JustCompare>(AO1->getPredicate()
	  					      ->getRHS(),
						      AO2->getPredicate()
						      ->getRHS());
	    if (JustCompare) {
	      if (ChildResult == 0)
	        return 0;	      		
	    } else {
	      if (ChildResult == NULL) {
		delete Result;
		return NULL;
	      }
	      Result->merge(*ChildResult);
	      delete ChildResult;
	    }
	  }
	}

	// Now we know we are handling with operators and may safely cast
	// and analyze its children
	const Operator *RO = dynamic_cast<const Operator*>(R),
	  *EO = dynamic_cast<const Operator*>(E);
	for (int I = 0, E = RO->getArity(); I != E; ++I) {
	  AnnotatedTreeList* ChildResult = 
	    MatchExpByRule<JustCompare>((*RO)[I], (*EO)[I]);

	  if (JustCompare) {
	    if (ChildResult == 0)
	      return 0;
	    else
	      continue;
	  }
	  
	  if (ChildResult == NULL) {
	    delete Result;
	    return NULL;
	  }

	  Result->merge(*ChildResult);	
	  delete ChildResult;
	}
	if (JustCompare) {	  
	  return reinterpret_cast<AnnotatedTreeList*>(1);
	}
	return Result;
      }
    // We can also match if a rule operand matches an expression operator
    if (R->isOperand() && E->isOperator())
      {
	const Operator *EO = dynamic_cast<const Operator*>(E);	
	//NOTE: MemRef is a special case not affected by wildcard rules
	if ((EO->getReturnTypeType() == R->getType() &&
	     (EO->getReturnTypeSize() == R->getSize() 
	      || R->getSize() == 0))
	    || (R->getType() == 0 && EO->getType() != MemRefOp)) 
	  {
	    if (JustCompare)
	      return reinterpret_cast<AnnotatedTreeList*>(1);
	    AnnotatedTreeList* Result = new AnnotatedTreeList();
	    const Operand* Op = dynamic_cast<const Operand*>(R);
	    AnnotatedTree AT(Op->getOperandName(), E);
	    Result->push_back(AT);
	    return Result;
	  }
      }
    if (JustCompare)
      return 0;
    return NULL;
  }

  // Traverse tree looking for leafs and its instantiation names.
  // When found, substitute the node with one provided by the
  // annotated tree list
  // Also, generate names for operands not matched, so as to avoid
  // using generic names defined in rules. When generating a name,
  // stores it in the anotated tree, so as to properly substitute
  // similar names with the same generated names.
  void SubstituteLeafs(Tree** T, AnnotatedTreeList* List) {
    if ((*T)->isOperator()) {
      AssignOperator *AO = dynamic_cast<AssignOperator*>(*T);
      if (AO != NULL && AO->getPredicate() != NULL) {
	  SubstituteLeafs(AO->getPredicate()->getLHSAddr(), List);
	  SubstituteLeafs(AO->getPredicate()->getRHSAddr(), List);
      }
      Operator *O = dynamic_cast<Operator*>(*T);
      for (int I = 0, E = O->getArity(); I != E; ++I)
	{
	  SubstituteLeafs(&(*O)[I], List);	   
	}
      return;
    }

    if ((*T)->isOperand()) {
      Operand *O = dynamic_cast<Operand*>(*T);
      bool Matched = false;
      for (AnnotatedTreeList::const_iterator I = List->begin(),
	     E = List->end(); I != E; ++I)
	{
	  if (I->first == O->getOperandName()) {
	    delete *T;
	    Matched = true;
	    *T = I->second->clone();
	    break;
	  }
	}
      if (!Matched) {
	std::string Name = O->getOperandName();
	std::stringstream SS;
	SS << Name << Rule::OpNum++;
	O->changeOperandName(SS.str());
	AnnotatedTree AT(Name, O);
	List->push_back(AT);
      }      
    }
  }

  // Match the Expression with pattern Patt1 and, if successfull,
  // transform it in Patt2. Otherwise, return NULL;
  Tree* Apply(const Tree *Patt1, const Tree* Patt2, const Tree* Expression)
  {
    AnnotatedTreeList *List = MatchExpByRule<false>(Patt1, Expression);

    // Match failed?
    if (List == NULL)
      return NULL;

    Tree *Result = Patt2->clone();
    SubstituteLeafs(&Result, List);
    delete List;

    return Result;
  }

  bool Rule::ForwardMatch(const expression::Tree* Expression) const {
    if (reinterpret_cast<long>(MatchExpByRule<true>(LHS, Expression)) == 1L)
      return true;
    return false;
  }

  bool Rule::BackwardMatch(const expression::Tree* Expression) const {
    if (reinterpret_cast<long>(MatchExpByRule<true>(RHS, Expression)) == 1L)
      return true;
    return false;
  }

  Tree* Rule::ForwardApply(const Tree* Expression) const
  {
    return Apply(LHS, RHS, Expression);
  }

  Tree* Rule::BackwardApply(const Tree* Expression) const
  {
    return Apply(RHS, LHS, Expression);
  }

  // Traverse tree looking for a decomposition operator type
  // and sever the tree.
  // XXX: Decomposition operators are deleted and must be top level
  // otherwise the behaviour is undefined and leaks may occur.
  std::list<Tree*>* SeverTree(Tree* T) {
    if (T->isOperator()) {
      Operator *O = dynamic_cast<Operator*>(T);

      if (T->getType() == DecompOp) {
	std::list<Tree*>* Result = new std::list<Tree*>();
	
	for (int I = 0, E = O->getArity(); I != E; ++I)
	  {
	    std::list<Tree*>* ChildResult = SeverTree((*O)[I]);
	    if (ChildResult == NULL) {
	      Result->push_back((*O)[I]);
	    } else {
	      Result->merge(*ChildResult);
	      delete ChildResult;
	    }
	  }
	
	O->detachNode();
	delete O;
	return Result;
      }
      
      //std::cerr << "A rule with an ordinary operator above an decomposition "
      //	<< "operator was found - illegal construction." << std::endl;  
    }

    return NULL;
  } 

  // Decompose an expression based on this rule (assuming it is
  // a decomposition rule and this rule applies to the expression)
  std::list<Tree*>* Rule::Decompose(const Tree* Expression) const
  {
    Tree *Transformed = NULL;
    if (Decomposition)
      Transformed = ForwardApply(Expression);
    else if (Composition)
      Transformed = BackwardApply(Expression);
    if (Transformed == NULL)
      return NULL;
  
    return SeverTree(Transformed);
  }

  void Rule::Print(std::ostream &S) const
  {
    LHS->print(S);
    if (Equivalence)
      S << "<=> ";
    else
      S << "=> ";
    RHS->print(S);
    S << ";";
  }

  // TransformationRules member functions
  bool TransformationRules::createRule(Tree* LHS,
				       Tree* RHS,
				       bool Equivalence) {
    Rule newRule(LHS, RHS, Equivalence, CurrentRuleNumber++);

    Rules.push_back(newRule);

    return true;
  }
  
  bool TransformationRules::createRule(expression::Tree* LHS, 
				       expression::Tree* RHS,
				       bool Equivalence,
				       OperandTransformationList &OList) {
    Rule newRule(LHS, RHS, Equivalence, CurrentRuleNumber++, OList);

    Rules.push_back(newRule);

    return true;
  }

  void TransformationRules::print(std::ostream &S) {
    S << "Rules Manager has a total of " << Rules.size()
	      << " rule(s).\n";
    S << "==================================\n";
    for (std::list<Rule>::const_iterator I = Rules.begin(), E = Rules.end();
	 I != E; ++I)
      {
        S << "Rule number " << I->RuleID << ": ";
	I->LHS->print(S);
	if (I->Equivalence)
	  S << "<=> ";
	else
	  S << "=> ";
	I->RHS->print(S);
	S << ";\n";
      }
    S << "\n";
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
