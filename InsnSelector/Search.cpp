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
#include "../Support.h"
#include <climits>
#include <cassert>

//#define DEBUG
#define DEBUG_SEARCH_RESULTS
#define USETRANSCACHE
//#define EXTENSIVESEARCH

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

  //Static member definition
#ifdef STATIC_TRANSCACHE      
  TransformationCache Search::TransCache;
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

  // Miscellaneous auxiliary functions
  
  // Special comparison function used in TransformationCache in order to assert
  // that two trees are identical
  bool CacheExactCompare(const Tree*E1, const Tree *E2) {
    bool isOperator;
    // Notes: We match if E2 implements the same operation with
    // greater data size - this means we imply that E1 is the
    // wanted expression and E2 is the implementation proposal
    if ((isOperator = E1->isOperator()) == E2->isOperator() 
	&& E1->getType() == E2->getType()
	&& E1->getSize() <= E2->getSize()) 
    {
      //Leaf
      if (!isOperator) {
	// Constant
	const Constant* C1 = dynamic_cast<const Constant*>(E1);
	const Constant* C2 = dynamic_cast<const Constant*>(E2);
	if ((C1 == NULL && C2 != NULL) || (C1 != NULL && C2 == NULL))
	  return false;
	if (C1 != NULL) {	    	    	    
	   if (C1->getConstValue() != C2->getConstValue())
	     return false;
	   else
	     return true;
	}
	// Immediate 
	const ImmediateOperand* I1 = 
	  dynamic_cast<const ImmediateOperand*>(E1);
	const ImmediateOperand* I2 = 
	  dynamic_cast<const ImmediateOperand*>(E2);
	if ((I1 == NULL && I2 != NULL) || (I1 != NULL && I2 == NULL)) 
	  return false;  
	if (I1 != NULL)
	  return true;
	// Register
	const Operand * O1 = dynamic_cast<const Operand*>(E1),
	    * O2 = dynamic_cast<const Operand*>(E2);
	if ((O1 == NULL && O2 != NULL) || (O1 != NULL && O2 == NULL)) 
	  return false;  
	// If both are specific references, they must match ref. names
	if (O1->isSpecificReference() && 
	    O1->getOperandName() != O2->getOperandName()) 
	  return false;	 	
	return true;
      }
      // Operator
      bool match = true;	
      // and analyze its children
      const Operator *O1 = dynamic_cast<const Operator*>(E1),
	*O2 = dynamic_cast<const Operator*>(E2);
      for (int I = 0, E = O1->getArity(); I != E; ++I) {
	if (!CacheExactCompare((*O1)[I], (*O2)[I])) {
	  match = false;
	  break;
	}
      }
      return match;
    }
    return false;
  }

  // This template is extensively used in the Search algorithm to 
  // conclude if two expressions are equal. If the template is instantiated
  // with JustTopLevel=true, the function doesn't descend into childrens, but
  // just compare the top level node.
  // RR is a pointer to a list of real registers used by E2 while matching
  // with E1's generic registers. If RR is NULL, this list is not updated.
  template <bool JustTopLevel>
  bool Compare (const Tree *E1, const Tree *E2, SearchRestrictions *ST)
  {
    bool isOperator;

    // Notes: We match if E2 implements the same operation with
    // greater data size - this means we imply that E1 is the
    // wanted expression and E2 is the implementation proposal
    if ((isOperator = E1->isOperator()) == E2->isOperator() &&
	((isOperator && (E1->getType() == E2->getType())) ||
	 (!isOperator && dynamic_cast<const Operand*>(E1)->getDataType() ==
	                 dynamic_cast<const Operand*>(E2)->getDataType())) &&
	E1->getSize() <= E2->getSize())
      {
	// Leaf?
	if (!isOperator) {
	  const Constant* C1 = dynamic_cast<const Constant*>(E1);
	  const Constant* C2 = dynamic_cast<const Constant*>(E2);
	  if ((C1 == NULL && C2 != NULL) || (C1 != NULL && C2 == NULL && 
	    dynamic_cast<const ImmediateOperand*>(E2) == NULL)) 
	    return false;
	  if (C1 != NULL && dynamic_cast<const ImmediateOperand*>(E2) != NULL)
	    {
	    if (ST != NULL) {
	      std::stringstream S;
	      S << C1->getConstValue();
	      return ST->AddToVRList(std::make_pair(C1->getOperandName(),
		S.str()));
	    } else {
	      return false;
	    }
	  }
	  // Depending on the operand type, we need to make further checkings
	  // Constants must be equal
	  if (C1 != NULL && C2 != NULL) {	    	    	    
	    if (C1->getConstValue() != C2->getConstValue())
	      return false;
	  }

	  // RegisterOperand handling
	  const RegisterOperand* RO1 = 
	    dynamic_cast<const RegisterOperand*>(E1);
	  const RegisterOperand* RO2 =
	    dynamic_cast<const RegisterOperand*>(E2);
	  if (RO1 != NULL && RO2 != NULL) {
	    if (RO1->getRegisterClass() != RO2->getRegisterClass())
	      return false;
	  }
	  // if semantic (RO2) is a register, check for specific registers
	  // being used and check if their reg. classes match
	  if (RO2 != NULL) {
	    const std::string *Name = 0;
	    if (ST != NULL) {
	      if (ST->LookupVR(dynamic_cast<const Operand*>(E1)
			       ->getOperandName(),
			       Name)) {
		if (!RO2->getRegisterClass()->hasRegisterName(*Name))
		  return false;
	      }	      
	      const RegisterClass *rclass = 0;
	      if (ST->LookupVC(dynamic_cast<const Operand*>(E1)
			       ->getOperandName(),
			       rclass)) {
		if (!(RO2->getRegisterClass() != rclass))
		  return false;
	      }	else {
		ST->AddToVCList(std::make_pair(dynamic_cast<
					       const Operand*>(E1)
					       ->getOperandName(),
					       RO2->
					       getRegisterClass()));
	      }
	    }
	  }	  

	  // Immediate handling
	  const ImmediateOperand* I1 = 
	    dynamic_cast<const ImmediateOperand*>(E1);
	  const ImmediateOperand* I2 = 
	    dynamic_cast<const ImmediateOperand*>(E2);
	  if ((I1 == NULL && I2 != NULL) || (I1 != NULL && I2 == NULL)) 
	    return false;  
	  
	  // References to specific registers must be equal
	  const Operand * O1 = dynamic_cast<const Operand*>(E1),
	    * O2 = dynamic_cast<const Operand*>(E2);
	  assert (O1 != NULL && O2 != NULL && "Nodes must be operands");
	  
	  if (O2->isSpecificReference() && !O1->isSpecificReference()) {
	    if (!O1->acceptsSpecificReference())
	      return false;
	    // It will return true only if the virtual register (specified
	    // by E1) is not already mapped to a different real reg.
	    if (ST != NULL) {
	      return ST->AddToVRList(std::make_pair(O1->getOperandName(),
						    O2->getOperandName()));
	    } else {
	      return false;
	    }
	    
	  }
	  // If both are specific references, they must match ref. names
	  if (O2->isSpecificReference() && O1->isSpecificReference() && 
	      O1->getOperandName() != O2->getOperandName()) 
	    return false;	      
	  
	  return true;
	} // end if (Leaf)	

	if (JustTopLevel)
	  return true;

	// Now we know we are handling with operators	
	bool match = true;	
	// and analyze its children
	const Operator *O1 = dynamic_cast<const Operator*>(E1),
	  *O2 = dynamic_cast<const Operator*>(E2);
	for (int I = 0, E = O1->getArity(); I != E; ++I) {
	  if (!Compare<false>((*O1)[I], (*O2)[I], ST)) {
	    match = false;
	    break;
	  }
	}
	return match;
      }
    return false;
  }

  // IsInVRFunctor member functions

  bool IsInVRFunctor::operator() (const Tree* A) {
    const Operand* O = dynamic_cast<const Operand*>(A);
    if (O != NULL) {
      for (VirtualToRealMap::const_iterator I = VR->begin(), E = VR->end();
	   I != E; ++I) {
	if (O->getOperandName() == I->first)
	  return false;	 
      }
    }
    return true;
  } 
  
  // SearchResult member functions

  SearchResult::SearchResult() {
    Instructions = new InstrList();
    Cost = INT_MAX;
    OperandsDefs = new OperandsDefsType();
    RulesApplied = new RulesAppliedList();
    ST = new SearchRestrictions();
    OpTrans = new OpTransLists();
  }

  SearchResult::~SearchResult() {
    delete Instructions;
    for (OperandsDefsType::iterator I = OperandsDefs->begin(),
	   E = OperandsDefs->end(); I != E; ++I)
      {
	delete *I;
      }
    delete OperandsDefs;
    delete RulesApplied;
    delete ST;
    delete OpTrans;
  }

  // Removes names from OperandsDefs list that are already assigned
  // to a real register (if it is preassigned, it is not an assembly
  // operand).
  void SearchResult::FilterAssignedNames() {
    VirtualToRealMap* VR = this->ST->getVR();
    if (VR == NULL)
      return;
    for (OperandsDefsType::iterator It = OperandsDefs->begin(), 
	   End = OperandsDefs->end(); It != End; ++It) {
      NameListType* In = *It;

      for (VirtualToRealMap::const_iterator I2 = VR->begin(), E2 = VR->end();
	   I2 != E2; ++I2) {
	if (!In->empty())
	  In->remove(I2->first);
      }
    
    }
  }

  // This member function checks if SearchResult has a real assignment
  // to Expression inputs. In this case, we may be trying to enforce 
  // restrictions on the expression that the SearchEngine user does not want. 
  bool SearchResult::CheckVirtualToReal(const Tree *Exp) const {
    VirtualToRealMap* VR = this->ST->getVR();
    if (VR == NULL)
      return true;
    return ApplyToLeafs<const Tree*, const Operator*, IsInVRFunctor>
      (Exp, IsInVRFunctor(VR));  
  }

  VirtualToRealMap::const_iterator 
  SearchResult::VRLookupName(std::string S) const {
    VirtualToRealMap* VR = this->ST->getVR();
    for (VirtualToRealMap::const_iterator I = VR->begin(), E = VR->end();
	 I != E; ++I) {
      if (I->first == S)
	return I;
    }
    return VR->end();
  } 

  void SearchResult::DumpResults(std::ostream& S) const {
    S << "\n==== Search results internal dump =====\nInstructions list:\n";
    for (InstrList::iterator I = Instructions->begin(), E = Instructions->end();
	 I != E; ++I) {
      S << I->first->getName();
      S << "\n";
    }
    S << "\nOperandsDefs list:\n";
    for (OperandsDefsType::iterator I = OperandsDefs->begin(), 
	   E = OperandsDefs->end(); I != E; ++I) {      
      for (NameListType::iterator I2 = (*I)->begin(), E2 = (*I)->end();
	   I2 != E2; ++I2) {
	S << *I2;
	S << " ";
      }
      S << "\n";
    }
    VirtualToRealMap* VirtualToReal = ST->getVR();
    if (VirtualToReal != NULL) {
	S << "\nVirtual to Real mapping list:\n";    
	for (VirtualToRealMap::const_iterator I = VirtualToReal->begin(),
		 E = VirtualToReal->end(); I != E; ++I) {
	    S << I->first << " <= " << I->second << "\n";
	}
    }
    S << "\nRules applied, by number:\n";
    for (RulesAppliedList::reverse_iterator I = RulesApplied->rbegin(),
	   E = RulesApplied->rend(); I != E; ++I) {
      S << *I;
      S << " ";
    }
    S << "\n=======================================\n\n";
  }


  // TransformationCache member functions
  // prime cache sizes: 1009 10007 103997
  // power-of-2 sizes: 1024 16384 131072 1048576
  TransformationCache::TransformationCache() : HASHSIZE(256) {
    HashTable = new CacheEntry* [HASHSIZE];
    assert (HashTable != NULL && "MemAlloc fail");
    for (unsigned I = 0, E = HASHSIZE; I != E; ++I) {
      HashTable[I] = NULL;
    }
  }
  
  TransformationCache::~TransformationCache() {
    unsigned size = 0;
    for (unsigned I = 0, E = HASHSIZE; I != E; ++I) {
      CacheEntry *p = HashTable[I];
      while (p) {
        CacheEntry *next = p->next;
        size++;
        delete p->LHS;
        delete p->RHS;
        delete p;
        p = next;
      }
    }
    std::cout << "Transcache size was: " << size << std::endl;
    delete [] HashTable;
  }

  inline void TransformationCache::Add(const Tree* Exp, const Tree* Target,
				       unsigned Depth) {
    unsigned Hash = Exp->getHash(Target->getHash()) % HASHSIZE;
    CacheEntry * const NewEntry = new CacheEntry();
    CacheEntry * const Cur = HashTable[Hash];
    NewEntry->LHS = Exp->clone();
    NewEntry->RHS = Target->clone();
    NewEntry->Depth = Depth;
    NewEntry->next = Cur;
    HashTable[Hash] = NewEntry;
    return;
  }

  inline TransformationCache::CacheEntry* TransformationCache::LookUp
  (const Tree* Exp, const Tree* Target, unsigned Depth) const {
    unsigned Hash = Exp->getHash(Target->getHash()) % HASHSIZE;
    CacheEntry* p = HashTable[Hash];
    while (p) {
      if (Depth <= p->Depth &&
	  CacheExactCompare(Target, p->RHS) &&
          CacheExactCompare(p->LHS, Exp))
	return p;
      p = p->next;
    }
    return NULL;
  }

  // SearchRestrictions member functions

  // Constructor
  SearchRestrictions::SearchRestrictions(VirtualToRealMap* vr,
					 VirtualClassesMap* vc){
    VR = vr;
    VC = vc;
    if (VR == 0)
      VR = new VirtualToRealMap();
    if (VC == 0)
      VC = new VirtualClassesMap();
  }

  SearchRestrictions::~SearchRestrictions() {
    setVR(0);
    setVC(0);
  }

  const VirtualToRealMap* SearchRestrictions::getVR() const {
    return VR;
  }
  
  const VirtualClassesMap* SearchRestrictions::getVC() const {
    return VC;
  }

  void SearchRestrictions::setVR(VirtualToRealMap* e) {
    if (VR)
      delete VR;
    VR = e;
  }

  void SearchRestrictions::setVC(VirtualClassesMap* e) {
    if (VC)
      delete VC;
    VC = e;
  }
  // SearchRestrictions' VirtualToRealMap related auxiliary functions

  // This auxiliary function adds an element to a Virtual-to-Real register
  // mapping. But if the virtual register is already mapped, returns false.
  inline
  bool SearchRestrictions::AddToVRList(RegPair Element) {
    if (!VR)
      VR = new VirtualToRealMap();
    for (VirtualToRealMap::const_iterator I = VR->begin(), E = VR->end();
	 I != E; ++I) {
      if (I->first == Element.first && I->second != Element.second)
	return false;
      else if (I->first == Element.first && I->second == Element.second)
	return true;
    }
    VR->push_back(Element);
    return true;
  }

  inline
  bool SearchRestrictions::LookupVR(const std::string& Key,
				    const std::string* Result) const {
    for (VirtualToRealMap::const_iterator I = VR->begin(), E = VR->end();
	 I != E; ++I) {
      if (I->first == Key) {
	Result = &I->second;
	return true;
      }
    }
    Result = 0;
    return false;
  }
    

  // Predicate function used to determine if two SearchResults define the
  // same virtual register in their VirtualToRealMap. If this happens,
  // the result may not be merged.
  inline
  bool SearchRestrictions::HasConflictingVRDefinitions(const 
						       VirtualToRealMap* B)
    const
  {
    const VirtualToRealMap *A = VR;
    if (A == NULL || B == NULL)
      return false;

    for (VirtualToRealMap::const_iterator I = A->begin(), E = A->end();
	 I != E; ++I) {
      for (VirtualToRealMap::const_iterator I2 = B->begin(), E2 = B->end();
	   I2 != E2; ++ I2) {
	if (I->first == I2->first && I->second != I2->second)
	  return true;
      }
    }

    return false;
  }

  inline
  bool SearchRestrictions::HasConflictingDefinitions(const 
						     SearchRestrictions* B)
    const {
    if (this == NULL)
      return false;
    if (B == NULL)
      return false;
    return (HasConflictingVRDefinitions(B->getVR()) ||
	    HasConflictingVCDefinitions(B->getVC()));
  }

  
  inline 
  void SearchRestrictions::MergeVRList(const VirtualToRealMap* Source){
    VirtualToRealMap *Destination = VR;
    if (Source != 0) 
      Destination->insert(Destination->begin(), Source->begin(),
			  Source->end());
  }

  // SearchRestrictions' VirtualClassesMap related auxiliary functions

  inline
  bool SearchRestrictions::AddToVCList(VCPair Element) {
    if (!VC)
      VC = new VirtualClassesMap();
    for (VirtualClassesMap::const_iterator I = VC->begin(), E = VC->end();
	 I != E; ++I) {
      if (I->first == Element.first && I->second != Element.second)
	return false;
      else if (I->first == Element.first && I->second == Element.second)
	return true;
    }
    VC->push_back(Element);
    return true;
  }

  // Predicate function used to determine if two SearchResults define the
  // same virtual register in their VirtualToRealMap. If this happens,
  // the result may not be merged.
  inline
  bool SearchRestrictions::HasConflictingVCDefinitions(const 
						       VirtualClassesMap* B)
  const
  {
    const VirtualClassesMap *A = VC;
    if (A == NULL || B == NULL)
      return false;

    for (VirtualClassesMap::const_iterator I = A->begin(), E = A->end();
	 I != E; ++I) {
      for (VirtualClassesMap::const_iterator I2 = B->begin(), E2 = B->end();
	   I2 != E2; ++ I2) {
	if (I->first == I2->first && I->second != I2->second)
	  return true;
      }
    }

    return false;
  }

  inline
  bool SearchRestrictions::LookupVC(const std::string& Key,
				    const RegisterClass* Result) const {
    for (VirtualClassesMap::const_iterator I = VC->begin(), E = VC->end();
	 I != E; ++I) {
      if (I->first == Key) {
	Result = I->second;
	return true;
      }
    }
    Result = 0;
    return false;
  }

  inline 
  void SearchRestrictions::MergeVCList(const VirtualClassesMap* Source){
    VirtualClassesMap *Destination = VC;
    if (Source != 0) 
      Destination->insert(Destination->begin(), Source->begin(),
			  Source->end());
  }

  inline 
  void SearchRestrictions::Merge(const SearchRestrictions* Source){
    if (Source != 0) {
      MergeVRList(Source->getVR());
      MergeVCList(Source->getVC());
    }
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
  
  inline bool CheckForConstInVRList(VirtualToRealMap *VR, 
				    const std::string &Name) {
    VirtualToRealMap::iterator I, E;
    for (I = VR->begin(), E = VR->end(); I != E; ++I) {
      if (I->first == Name) {
	break;
      }
    }
    // Not found
    if (I == VR->end())
      return false;
    VR->erase(I);
    return true;
  }

  // This auxiliary function searches for leafs in an expression and
  // gets their names and put they in a list.
  // With this information, we know what names to use as operands
  // of instructions.
  NameListType* ExtractLeafsNames(const Tree* Exp, VirtualToRealMap *VR) {
    // Sanity check
    if (Exp == NULL)
      return NULL;
    
    NameListType* Result = new NameListType();

    // A leaf
    if (Exp->isOperand()) {
      // Constants may be emitted as operands in special cases identified
      // with help of VirtualToRealMap.
      const Constant* CExp;
      if ((CExp = dynamic_cast<const Constant*>(Exp)) != NULL) {
	if (!CheckForConstInVRList(VR, CExp->getOperandName())) 
	  return Result;
	// This constant was bound to an immediate and must be emmited as 
	// operand
	std::stringstream SS;
	SS << "CONST<" << CExp->getConstValue() << ">";
	Result->push_back(SS.str());
	return Result;
      }
      // We should not expect fragments to be present here
      assert(dynamic_cast<const FragOperand*>(Exp) == NULL &&
	     "Unexpected node type.");
      const Operand* O = dynamic_cast<const Operand*>(Exp);
      Result->push_back(O->getOperandName());
      return Result;
    }

    // An operator
    const Operator* O = dynamic_cast<const Operator*>(Exp);    
    for (int I = 0, E = O->getArity(); I != E; ++I) 
      {
	NameListType* ChildResult = ExtractLeafsNames((*O)[I], VR);
	assert (ChildResult != NULL && "Must return a valid pointer");
	Result->splice(Result->end(), *ChildResult);
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

  // Extracts an expression's primary operator type
  inline unsigned PrimaryOperatorType (const Tree* Expression)
  {
    if (!Expression->isOperator())
      return 0;

    const Operator* O = dynamic_cast<const Operator*>(Expression);
    if (O->getType() == AssignOp) {      
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
    Destination->RulesApplied->splice(Destination->RulesApplied->begin(),
				      *(Source->RulesApplied));    
    Destination->OpTrans->splice(Destination->OpTrans->begin(),
				 *(Source->OpTrans));    
    Destination->ST->getVR()->splice(Destination->ST->getVR()->begin(),
				     *(Source->ST->getVR()));
    Destination->ST->getVC()->splice(Destination->ST->getVC()->begin(),
				     *(Source->ST->getVC()));

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
  // in the SearchResult record. As we do not know yet the instruction
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
					       unsigned CurDepth,
					       const SearchRestrictions *ST)
  {
    if (!R->Decomposition && !R->Composition)
      return NULL;

    std::list<Tree*>* DecomposeList = R->Decompose(Expression);
    if (DecomposeList == NULL)
      return NULL;

    SearchResult* CandidateSolution = new SearchResult();
    if (ST != NULL)
      CandidateSolution->ST->MergeVRList(ST->getVR());

    for (std::list<Tree*>::reverse_iterator I = DecomposeList->rbegin(),
	   E = DecomposeList->rend(); I != E; ++I)
      {
	// Check for goal
	// XXX: Need a better checking of goal! Sometimes a wrong tree
	// will be matched as goal and all is lost!
	if (Goal != NULL) {
	  SearchRestrictions *STnew = new SearchRestrictions();
	  STnew->MergeVRList(CandidateSolution->ST->getVR());
	  if (Compare<true>(Goal, *I, STnew) == true
	      && !STnew->HasConflictingDefinitions(CandidateSolution->ST) 
	      && MatchedGoal == NULL) {	    
	    delete CandidateSolution->ST;
	    CandidateSolution->ST = STnew;
	    MatchedGoal = (*I)->clone();
	    continue;
	  } else {
	    delete STnew;
	  }
	}
	SearchResult* ChildResult = (*this)(*I, CurDepth + 1, 
					    CandidateSolution->ST);
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
				      unsigned CurDepth,
				      const SearchRestrictions *ST) {    
    // First check the top level node
    SearchRestrictions *STnew = new SearchRestrictions();
    if (!(Compare<true>(Transformed, InsnSemantic, STnew) &&
        !ST->HasConflictingDefinitions(STnew))) {
      delete STnew;
      return false;
    }
    
    // Transformation revealed that these expressions match directly
    if (Transformed->isOperand()) {
      Result->Cost = 0;
      delete Result->ST;
      Result->ST = STnew;
      UpdateCurrentOperandDefinition(Result, 
				     ExtractLeafsNames(Transformed,
						       STnew->getVR()));
      return true;
    }
    delete STnew;

    // Now check for guarded assignments before delving into each children
    // of this operator
    SearchResult* TempResults = new SearchResult();
    
    // Now for every operator, prove their children equal
    const Operator* O = dynamic_cast<const Operator*>(Transformed);
    const Operator* OIns = dynamic_cast<const Operator*>(InsnSemantic);
    TempResults->ST->Merge(ST);
    for (int I = 0, E = O->getArity(); I != E; ++I) 
      {
	SearchResult* SRChild = 
	  TransformExpression((*O)[I], (*OIns)[I], CurDepth+1,
			      TempResults->ST);
	
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
					    unsigned CurDepth, 
					    const SearchRestrictions *ST)
  {   
    //DbgIndent(CurDepth);
    Dbg(for (unsigned i = 0; i < CurDepth; ++i) std::cerr << "*";);
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

#ifndef EXTENSIVESEARCH
    const unsigned PO = PrimaryOperatorType(Expression);

    // Prune unworthy trials (heuristic)
    if (!HasCloseSemantic(PrimaryOperatorType(InsnSemantic), PO)) {
      DbgIndent(CurDepth);
      DbgPrint("CloseSemantic heuristic pruned this trial.\n");
      return Result;
    }
#endif

    // See if we already have a match
    SearchRestrictions *STnew = new SearchRestrictions();
    if (Compare<false>(Expression, InsnSemantic, STnew) == true &&
	!ST->HasConflictingDefinitions(STnew)) {
      DbgIndent(CurDepth);
      DbgPrint("Already matches!\n");
      Result->Cost = 0;
      delete Result->ST;
      Result->ST = STnew;
      UpdateCurrentOperandDefinition(Result, 
				     ExtractLeafsNames(Expression,
						       STnew->getVR()));
      return Result;
    }      
    delete STnew;

    DbgIndent(CurDepth);
    DbgPrint("Trying to transform children without applying any rule");   
    DbgPrint(" at the top level node...\n");

    // See if we obtain success without applying a transformation
    // at this level
    if (TransformExpressionAux(Expression, InsnSemantic, Result, CurDepth, ST) 
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
#ifndef EXTENSIVESEARCH	
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
#else
	if (!I->ForwardMatch(Expression)) 
	  {
	    if (!I->BackwardMatch(Expression))
	      continue;
	    Forward = false;
	  }
#endif

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

#ifdef EXTENSIVESEARCH
	  // Try to chain other transformations at the same level (without
	  // descending to children), limiting the number of trials by
	  // successfully increasing CurDepth.
	  // This recursive call eventually will also test for children
	  // transformations because of a call to TransformExpressionAux 
	  // early in this function.
	  SearchResult* SRChild = 
	    TransformExpression(Transformed, InsnSemantic, CurDepth+1, ST);
	  // If success
	  if (SRChild != NULL && SRChild->Cost != INT_MAX) {
	    delete Result;
	    Result = SRChild;
	    Result->RulesApplied->push_back(I->RuleID);
	    if (Forward)
	      Result->OpTrans
		->push_back(I->ForwardApplyGetOpTrans(Expression));
	    else
	      Result->OpTrans
		->push_back(I->BackwardApplyGetOpTrans(Expression));
	    delete Transformed;
	    return Result;
	  }
	  // Failed
	  if (SRChild != NULL)
	    delete SRChild;
#else
	  // Calls auxiliary to confirm that the transformed expression
	  // equals our goal
	  // This may involve recursive calls to this function (to transform
	  // and adapt the children nodes).
	  if (TransformExpressionAux(Transformed, InsnSemantic, Result, 
				     CurDepth, ST) == true)
	    {	      
	      Result->RulesApplied->push_back(I->RuleID);
	      if (Forward)
		Result->OpTrans
		  ->push_back(I->ForwardApplyGetOpTrans(Expression));
	      else
		Result->OpTrans
		  ->push_back(I->BackwardApplyGetOpTrans(Expression));
	      delete Transformed;
	      return Result;
	    }	  
#endif
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
				 CurDepth, ST);
	
	//Failed
	if (ChildResult == NULL || ChildResult->Cost == INT_MAX) {	  
	  if (Transformed != NULL)
	    delete Transformed;
	  continue;
	}
	
	ChildResult->ST->Merge(ST);
	
	// Now we decomposed and have a implementation of the remaining
	// trees. We just need to assert that the transformed tree
	// is really what we want.
	// Calls auxiliary to confirm that the transformed expression
	// equals our goal
	// This may involve recursive calls to this function (to transform
	// and adapt the children nodes).
	if (TransformExpressionAux(Transformed, InsnSemantic, Result,
				   CurDepth, ChildResult->ST) == true)
	  {
	    DbgIndent(CurDepth);
	    DbgPrint("Decomposition was successful\n");	    
	    MergeSearchResults(Result, ChildResult);
	    delete ChildResult;
	    delete Transformed;
	    Result->RulesApplied->push_back(I->RuleID);
	    if (Forward)
		Result->OpTrans
		  ->push_back(I->ForwardApplyGetOpTrans(Expression));
	    else
		Result->OpTrans
		  ->push_back(I->BackwardApplyGetOpTrans(Expression));
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
  // VR is a mapping with current bindings of virtual registers (operand names)
  // to real registers, so we need to avoid redefinitions when searching
  // for an implementation of Expression.
  SearchResult* Search::operator() (const Tree* Expression, unsigned CurDepth,
				    const SearchRestrictions *ST)
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
	    SearchRestrictions* STnew = new SearchRestrictions();
	    if (Compare<false>(Expression, I2->SemanticExpression,
			       STnew) && 
		!STnew->HasConflictingDefinitions(ST) &&
		Result->Cost >= (*I)->getCost())
	      {
		delete Result;		
		Result = new SearchResult();
		Result->Cost = (*I)->getCost();
		Result->Instructions->push_back(std::make_pair(*I,I2));
		delete Result->ST;
		Result->ST = STnew;
		UpdateCurrentOperandDefinition(Result, 
					       ExtractLeafsNames
					       (Expression,
						STnew->getVR()));
		break;
	      } else {
	      delete STnew;
	    }
	  }
      }

    // If found something, return it
    if (Result->Cost != INT_MAX) {
      DbgIndent(CurDepth);
      DbgPrint("Direct match successful\n");
#ifdef DEBUG_SEARCH_RESULTS
      if (CurDepth == 0) {
	Result->DumpResults(std::cerr);
      }
#endif
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
								 CurDepth, ST);

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
	      TransformExpression(Expression, I1->SemanticExpression,
				  CurDepth, ST);

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
    if (Result->Cost != INT_MAX) {
#ifdef DEBUG_SEARCH_RESULTS
      if (CurDepth == 0) {
	Result->DumpResults(std::cerr);
      }
#endif
      return Result;    
    }

    // We can't find anything
    return Result;
  }

}
