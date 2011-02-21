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
    //NOTE: Transfer destinations are not affected by wildcard rules
    if ((isOperator = R->isOperator()) == E->isOperator() &&
	((R->getType() == E->getType() &&
	(R->getSize() == E->getSize() || R->getSize() == 0 || 
	 E->getSize() == 0)) ||
	 (R->getType() == 0 && (E->isOperator() == false 
	                     || E->getType() != MemRefOp)
	                     && E->isTransferDestination() == false) ) )
      {
	AnnotatedTreeList* Result = NULL;
	if (!JustCompare) Result = new AnnotatedTreeList();

	// Leaf? Cast to operand
	if (!isOperator) {	
	  // If not wildcard, then it must be a perfect match.
	  if (R->getType() != 0) {
	    
	    const Constant* C1 = dynamic_cast<const Constant*>(R);
	    const Constant* C2 = dynamic_cast<const Constant*>(E);
	    // If rule is not constant, it may as well match a constant
	    if (C1 != NULL && C2 == NULL) {
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
	//NOTE: Transfer destinations are not affected by wildcard rules
	if ((EO->getReturnTypeType() == R->getType() &&
	     (EO->getReturnTypeSize() == R->getSize() 
	      || R->getSize() == 0))
	    || (R->getType() == 0 && EO->getType() != MemRefOp 
	        && !EO->isTransferDestination())) 
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
  bool SubstituteRoot(Tree** T, AnnotatedTreeList* List,
		       const OperandTransformationList& OpTransList) {
    if ((*T)->isOperand()) {
      Operand *O = dynamic_cast<Operand*>(*T);
      bool Matched = false;
      for (AnnotatedTreeList::const_iterator I = List->begin(),
	     E = List->end(); I != E; ++I)
	{
	  if (I->first == O->getOperandName()) {	    
	    Matched = true;	    
	    delete *T;
	    *T = I->second->clone();	    
	    break;
	  }
	}
      return Matched;
    }
    return false;
  }

  // Traverse tree looking for leafs and its instantiation names.
  // When found, substitute the node with one provided by the
  // annotated tree list
  // Also, generate names for operands not matched, so as to avoid
  // using generic names defined in rules. When generating a name,
  // stores it in the anotated tree, so as to properly substitute
  // similar names with the same generated names.
  void SubstituteLeafs(Tree* T, AnnotatedTreeList* List,
		       const OperandTransformationList& OpTransList, 
		       Operator* Parent = 0, int ChildIndex = -1) {
    if (T->isOperator()) {      
      Operator *O = dynamic_cast<Operator*>(T);
      for (int I = 0, E = O->getArity(); I != E; ++I)
	{
	  SubstituteLeafs((*O)[I], List, OpTransList, O, I);
	}
      return;
    }

    if (T->isOperand()) {
      Operand *O = dynamic_cast<Operand*>(T);
      bool Matched = false;
      for (AnnotatedTreeList::const_iterator I = List->begin(),
	     E = List->end(); I != E; ++I)
	{
	  if (I->first == O->getOperandName()) {	    
	    Matched = true;
	    assert (Parent != 0 && "Cannot change root node");
	    Parent->setChild(ChildIndex, I->second->clone());
	    delete T;
	    break;
	  }
	}
      if (Matched)
	return;
      // Trying operand transformation bindings      
      std::string Name;
      bool Found = false;
      for (OperandTransformationList::const_iterator I = OpTransList.begin(),
	    E = OpTransList.end(); I != E; ++I) {
	if (I->RHSOperand == O->getOperandName()) {
	  Name = I->LHSOperand;
	  Found = true;
	  break;
	}
      }
      if (Found) {
	for (AnnotatedTreeList::const_iterator I = List->begin(),
	    E = List->end(); I != E; ++I)
	{
	  if (I->first == Name) {
	    std::stringstream SS;
	    const Operand* Opand = dynamic_cast<const Operand*>(I->second);
	    assert(Opand != NULL && "OpTrans. must refer to operand");
	    Matched = true;
	    SS << Opand->getOperandName() << "_" << O->getOperandName();
	    O->changeOperandName(SS.str());
	    break;
	  }
	}
	assert(Matched && "Invalid OpTrans. binding");        
      }
      if (Matched)
	return;
      // Otherwise...     
      std::string OldName = O->getOperandName();
      std::stringstream SS;
      SS << OldName << Rule::OpNum++;
      O->changeOperandName(SS.str());
      AnnotatedTree AT(OldName, O);
      List->push_back(AT);            
    }
  }

  // Match the Expression with pattern Patt1 and, if successfull,
  // transform it in Patt2. Otherwise, return NULL;
  Tree* Apply(const Tree *Patt1, const Tree* Patt2, const Tree* Expression,
	      const OperandTransformationList& OpTransList)
  {
    AnnotatedTreeList *List = MatchExpByRule<false>(Patt1, Expression);

    // Match failed?
    if (List == NULL)
      return NULL;

    Tree *Result = Patt2->clone();
    if (!SubstituteRoot(&Result, List, OpTransList))
      SubstituteLeafs(Result, List, OpTransList);
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
    return Apply(LHS, RHS, Expression, OpTransList);
  }

  Tree* Rule::BackwardApply(const Tree* Expression) const
  {
    return Apply(RHS, LHS, Expression, OpTransList);
  }
  
  OperandTransformationList Rule::ApplyGetOpTrans(const Tree* Patt1, 
						  const Tree* Exp) const
  {
    OperandTransformationList Result(OpTransList);
    AnnotatedTreeList *List = MatchExpByRule<false>(Patt1, Exp);
    for (OperandTransformationList::iterator I = Result.begin(), 
           E = Result.end(); I != E; ++I) {
      bool Found = false;
      for (AnnotatedTreeList::iterator I2 = List->begin(), E2 = List->end();
           I2 != E2; ++I2) {
	if (I->LHSOperand == I2->first) {
	  const Operand* Node = dynamic_cast<const Operand*>(I2->second);
	  assert (Node != NULL && "OpTransform. rule must refer to operand");	  
	  //BUG: Dangerous! May cause infinite loop
	  I->TransformExpression = 
	    I->PatchTransformExpression(Node->getOperandName());	    
	  I->LHSOperand = Node->getOperandName();
	  Found = true;
	  break;
	}
      }
      assert (Found && "OpTransform. referring to inexistent operand?");
    }
    return Result;
  }
  
  OperandTransformationList Rule::ForwardApplyGetOpTrans(const Tree* Exp) const
  {
    return ApplyGetOpTrans(LHS, Exp);
  }
  
  OperandTransformationList Rule::BackwardApplyGetOpTrans(const Tree* Exp) 
  const
  {
    return ApplyGetOpTrans(RHS, Exp);
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
