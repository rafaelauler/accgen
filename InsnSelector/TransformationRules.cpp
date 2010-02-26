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

namespace backendgen {

  using namespace backendgen::expression;

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
  Rule::Rule(Tree* LHS, Tree* RHS, bool Equivalence):
    LHS(LHS), RHS(RHS), Equivalence(Equivalence)
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
    if ((isOperator = R->isOperator()) == E->isOperator() &&
	((R->getType() == E->getType() &&
	(R->getSize() == E->getSize() || R->getSize() == 0 || 
	 E->getSize() == 0)) ||
	 R->getType() == 0) )
      {
	AnnotatedTreeList* Result = NULL;
	if (!JustCompare) Result = new AnnotatedTreeList();

	// Leaf? Cast to operand
	if (!isOperator) {	
	  if (JustCompare) {	    
	    return reinterpret_cast<AnnotatedTreeList*>(1);
	  }
	  const Operand* Op = dynamic_cast<const Operand*>(R);
	  AnnotatedTree AT(Op->getOperandName(), E);
	  Result->push_back(AT);
	  return Result;
	}
	
	bool match = true;
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
	if ((EO->getReturnTypeType() == R->getType() &&
	     (EO->getReturnTypeSize() == R->getSize() 
	      || R->getSize() == 0))
	    || R->getType() == 0) 
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
	SS << Name << rand();
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
    if (reinterpret_cast<int>(MatchExpByRule<true>(LHS, Expression)) == 1)
      return true;
    return false;
  }

  bool Rule::BackwardMatch(const expression::Tree* Expression) const {
    if (reinterpret_cast<int>(MatchExpByRule<true>(RHS, Expression)) == 1)
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
    Rule newRule(LHS, RHS, Equivalence);

    Rules.push_back(newRule);
  }

  void TransformationRules::print(std::ostream &S) {
    S << "Rules Manager has a total of " << Rules.size()
	      << " rule(s).\n";
    S << "==================================\n";
    for (std::list<Rule>::const_iterator I = Rules.begin(), E = Rules.end();
	 I != E; ++I)
      {
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
