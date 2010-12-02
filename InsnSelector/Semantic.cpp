//===- Semantic.cpp - Implementation file for semantic    --*- C++ -*------===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// These classes represent the instruction behaviour, as perceived by the
// compiler backend generator tool.
//
//===----------------------------------------------------------------------===//
#include "Semantic.h"
#include "../Support.h"
#include <functional>
#include <cassert>

namespace backendgen {

  namespace expression {
    
    // Generic hash function based on ELF's hash function for symbol names.
    template<typename T> inline
    unsigned int hash (const T &Name, unsigned hash_in) {
      unsigned long int Hash = hash_in;
      typename T::const_iterator I = Name.begin(), 
	E = Name.end();
      
      while (I != E) {
	unsigned long int Hi;
	Hash = (Hash << 4) + *I++;
	Hi = Hash & 0xf0000000;
	Hash ^=  Hi;
	Hash ^=  Hi >> 24;
      }
      
      return Hash;
    }
    
    const std::string anystr ("ANY");

    const std::string& 
    OperandTableManager::getTypeName (const OperandType &OpType) {
      if (OpType.Type == 0)
	return anystr;
      // If the element does not exists, it gets created!
      // So assert it exists, otherwise it is an error to call this member
      // function      
      assert(ReverseTypeMap.find(OpType) != ReverseTypeMap.end() &&
	     "Operand type must exist");
      return ReverseTypeMap[OpType];
    }
    
    ConstType OperandTableManager::parseCondVal (const std::string &S) {
      if (S == CondLtStr)
	return LtCondVal;
      if (S == CondGtStr)
	return GtCondVal;
      if (S == CondUltStr)
	return UltCondVal;
      if (S == CondUgtStr)
	return UgtCondVal;
      if (S == CondNeStr)
	return NeCondVal;
      if (S == CondEqStr)
	return EqCondVal;
      return 0;
    }
    
    OperandType OperandTableManager::getType (const std::string &Name) {    
      
      if (TypeMap.find(Name) == TypeMap.end()) {
	OperandType NewType;

	NewType.Size = 32;

	// Try to recognize known types and use the adequate code
	if (Name == IntTypeStr)
	  NewType.Type = IntType;
	else if (Name == CondTypeStr) {
	  NewType.Type = CondType;
	} else {
	  NewType.Type = hash<std::string>(Name, 0);	
	  if (NewType.Type < LastType)
	    NewType.Type += LastType;
	}

	// Avoiding collisions
	while (ReverseTypeMap.find(NewType) != ReverseTypeMap.end()) {
	  ++NewType.Type;	  
	}

	NewType.DataType = NewType.Type;
	
	TypeMap[Name] = NewType;
	ReverseTypeMap[NewType] = Name;
      }
      
      return TypeMap[Name];
    }

    int OperandTableManager::updateSize (OperandType Type, 
					 unsigned int NewSize) {
      std::string Name;
      ReverseTypeMapType::iterator Iter = 
	ReverseTypeMap.find(Type);

      if (Iter == ReverseTypeMap.end())
	return -1;

      Name = (*Iter).second;
      ReverseTypeMap.erase(Iter);      
      Type.Size = NewSize;
      ReverseTypeMap[Type] = Name;
      TypeMap[Name] = Type;

      return 0;
    }

    void OperandTableManager::printAll (std::ostream& S) const {
      S << "Operand Manager has a total of " << TypeMap.size()
	<< " operand(s).\n";
      S << "==================================\n";
      for (TypeMapType::const_iterator I = TypeMap.begin(), E = TypeMap.end();
	   I != E; ++I)
	{
	  S << "Name: " << I->first << " Type: " << I->second.Type
	    << " Size: " << I->second.Size << " DataType: " 
	    << I->second.DataType << "\n";
	}
      S << "\n";
    }

    void OperandTableManager::setCompatible (const std::string &O1,
					     const std::string &O2) {
      OperandType Type = getType(O1);
      Type.DataType = getType(O2).DataType;
      TypeMap[O1] = Type;
    }
    
    OperatorType OperatorTableManager::getType (const std::string &TypeName) {
      
      if (OperatorMap.find(TypeName) == OperatorMap.end()) {
	OperatorType NewType;
	
	NewType.Arity = 0;
	// Try to recognize known operators and use the adequate code
	if (TypeName == AddOpStr)
	  NewType.Type = AddOp;
	else if (TypeName == SubOpStr)
	  NewType.Type = SubOp;
	else if (TypeName == NegOpStr)
	  NewType.Type = NegOp;
	else if (TypeName == DecompOpStr)
	  NewType.Type = DecompOp;
	else if (TypeName == IfOpStr)
	  NewType.Type = IfOp;
	else if (TypeName == AssignOpStr)
	  NewType.Type = AssignOp;
	else if (TypeName == MemRefOpStr)
	  NewType.Type = MemRefOp;
	else if (TypeName == CallOpStr)
	  NewType.Type = CallOp;
	else if (TypeName == ReturnOpStr)
	  NewType.Type = ReturnOp;
	else { // User defined operator
	  NewType.Type = hash<std::string>(TypeName, 0);
	  if (NewType.Type < LastOp)
	    NewType.Type += LastOp;
	}

	// Avoiding collisions
	while (ReverseOperatorMap.find(NewType) != ReverseOperatorMap.end()) {
	  ++NewType.Type;
	}

	OperatorMap[TypeName] = NewType;
	ReverseOperatorMap[NewType] = TypeName;
      }

      return OperatorMap[TypeName];
    }

    void OperatorTableManager::setAlias (const std::string &Op1,
					 const std::string &Op2) {
      OperatorMap[Op1] = getType(Op2);
    }

    int OperatorTableManager::updateArity (OperatorType Type, 
					   int NewArity) {
      std::string Name;
      ReverseOperatorMapType::iterator Iter = 
	ReverseOperatorMap.find(Type);

      if (Iter == ReverseOperatorMap.end())
	return -1;

      Name = (*Iter).second;
      ReverseOperatorMap.erase(Iter);      
      Type.Arity = NewArity;
      ReverseOperatorMap[Type] = Name;
      OperatorMap[Name] = Type;

      return 0;
    }

    std::string OperatorTableManager::getOperatorName (OperatorType type) {
      return ReverseOperatorMap[type];
    }

    // Operator member functions
    Operator* Operator::BuildOperator(OperatorTableManager &Man,
				      OperatorType OpType) {
      if (OpType.Type == AssignOp)
	return new AssignOperator(Man, NULL, NULL, NULL);
      return new Operator(Man, OpType);
    }

    void Operator::setReturnType (const OperandType &OType) {
      ReturnType = OType;
    }

    unsigned Operator::getHash(unsigned hash_chain) const {
      for (unsigned I = 0, E = Type.Arity; I != E; ++I) {
	hash_chain = Children[I]->getHash(hash_chain);
      }
      hash_chain = (hash_chain << 4) + Type.Type;
      unsigned Hi = hash_chain & 0xf0000000;
      hash_chain ^=  Hi;
      hash_chain ^=  Hi >> 24;      
      return hash_chain;
    }

    // Operand member functions     

    unsigned Operand::getHash(unsigned hash_chain) const {
      return hash<std::string>(OperandName, hash_chain);
    }

    // Contant member functions
    unsigned Constant::SeqNum = 0;

    Constant::Constant (OperandTableManager& Man, const ConstType Val,
			const OperandType &Type):
      Operand(Man, Type, "C")
    {
      std::stringstream SS;
      Value = Val;
      SS << "CONST_" << SeqNum++;
      this->OperandName = SS.str();
    }

    // Register member functions
    Register::Register(const std::string &RegName) {
      Name = RegName;
    }

    void Register::addSubClass(Register *Reg) {
      SubClasses.push_back(Reg);
    }

    Register::ConstIterator Register::getSubsBegin() const {
      return SubClasses.begin();
    }
    
    Register::ConstIterator Register::getSubsEnd() const {
      return SubClasses.end();
    }

    const std::string &Register::getName() const {
      return Name;
    }

    // RegisterClass member functions
    RegisterClass::RegisterClass(const std::string &ClassName)
    {
      Name = ClassName;
    }

    RegisterClass::RegisterClass(const std::string &ClassName,
				 const OperandType &OpType) {      
      Name = ClassName;
      Type = OpType;
    }

    bool RegisterClass::addRegister(Register *Reg) {
      return Registers.insert(Reg).second;
    }

    bool RegisterClass::hasRegister(const Register *Reg) {
      Register *Reg2 = const_cast<Register*>(Reg);
      return (Registers.find(Reg2) != Registers.end());
    }

    bool RegisterClass::hasRegisterName(const std::string &RegName) {
      for (ConstIterator I = getBegin(), E = getEnd(); I != E; ++I) {
	Register *Element = *I;
	if (Element->getName() == RegName)
	  return true;
	for (Register::ConstIterator I2 = Element->getSubsBegin(), 
	       E2 = Element->getSubsEnd(); I2 != E2; ++I2) {
	  Register *SubEl = *I2;
	  if (SubEl->getName() == RegName)
	    return true;
	}
      }
      return false;
    }

    
    const std::string &RegisterClass::getName() const {
      return Name;
    }

    OperandType RegisterClass::getOperandType() const {
      return Type;
    }
    
    RegisterClass::ConstIterator RegisterClass::getBegin() const {
      return Registers.begin();
    }
    
    RegisterClass::ConstIterator RegisterClass::getEnd() const {
      return Registers.end();
    }


    // RegClassManager member functions
    
    RegClassManager::RegClassManager()
    {
      ProgramCounter = NULL;
      ReturnRegister = NULL;
      FramePointer = NULL;
      StackPointer = NULL;
      GrowsUp = false;
      Alignment = 0;
      PCOffset = 0;
    }


    RegClassManager::~RegClassManager() {
      // Deleting all registers
      for (std::set<Register*>::iterator I = Registers.begin(),
	     E = Registers.end(); I != E; ++I)
	{
	  Register *pointer = *I;
	  delete pointer;
	}
      // Deleting all register classes
      for (std::set<RegisterClass*>::iterator I = RegClasses.begin(),
	     E = RegClasses.end(); I != E; ++I)
	{
	  RegisterClass *pointer = *I;
	  delete pointer;
	}
      // Deleting all calling conventions
      for (std::list<CallingConvention*>::iterator I = CallConvs.begin(),
	     E = CallConvs.end(); I != E; ++I) 
	{
	  CallingConvention *pointer = *I;
	  delete pointer;
	}
    }

    bool RegClassManager::addRegClass(RegisterClass *RegClass) {
      return RegClasses.insert(RegClass).second;
    }

    bool RegClassManager::addRegister(Register *Reg) {
      return Registers.insert(Reg).second;
    }

    // Adds one register to the list of callee saved registers.
    // This register must also exist in the normal register list, 
    // as this is the list used for registers definitions in the LLVM backend.
    bool RegClassManager::addCalleeSaveRegister(Register *Reg) {
      return CalleeSaveRegisters.insert(Reg).second;
    }

    void RegClassManager::addCallingConvention(CallingConvention* Elem) {
      CallConvs.push_back(Elem);
    }
    
    void RegClassManager::setProgramCounter(const Register* Reg)
    {
      ProgramCounter = Reg;
    }
    
    void RegClassManager::setReturnRegister(const Register* Reg)
    {
      ReturnRegister = Reg;
    }
    
    void RegClassManager::setStackPointer(const Register* Reg)
    {
      StackPointer = Reg;
    }
    
    void RegClassManager::setFramePointer(const Register* Reg)
    {
      FramePointer = Reg;
    }
    
    const Register* RegClassManager::getProgramCounter() const {
      return ProgramCounter;
    }
    
    const Register* RegClassManager::getReturnRegister() const {
      return ReturnRegister;
    }
    
    const Register* RegClassManager::getFramePointer() const {
      return FramePointer;
    }
    
    const Register* RegClassManager::getStackPointer() const {
      return StackPointer;
    }

    // Adds one register to the list of reserved registers. Same restrictions
    // above apply.
    bool RegClassManager::addReservedRegister(Register *Reg) {
      return ReservedRegisters.insert(Reg).second;
    }
    
    bool RegClassManager::addAuxiliarRegister(Register *Reg) {
      return AuxiliarRegisters.insert(Reg).second;
    }

    RegisterClass *RegClassManager::getRegClass(const std::string &ClassName) {
      for (std::set<RegisterClass*>::iterator I = RegClasses.begin(),
	     E = RegClasses.end(); I != E; ++I)
	{
	  RegisterClass *pointer = *I;
	  if (pointer->getName() == ClassName)
	    return pointer;
	}
      return NULL;
    }

    Register *RegClassManager::getRegister(const std::string &RegisterName) {
      for (std::set<Register*>::iterator I = Registers.begin(),
	     E = Registers.end(); I != E; ++I)
	{
	  Register *pointer = *I;
	  if (pointer->getName() == RegisterName)
	    return pointer;
	}
      return NULL;    
    }

    RegisterClass *RegClassManager::getRegRegClass(const Register* Reg) {
      for (std::set<RegisterClass*>::iterator I = RegClasses.begin(),
	     E = RegClasses.end(); I != E; ++I)
	{
	  RegisterClass *pointer = *I;
	  if (pointer->hasRegister(Reg))
	    return pointer;
	}
      return NULL;
    }     

    std::set<RegisterClass*>::const_iterator RegClassManager::getBegin() const
    {
      return RegClasses.begin();
    }

    std::set<RegisterClass*>::const_iterator RegClassManager::getEnd() const 
    {
      return RegClasses.end();
    }

    std::set<Register*>::const_iterator RegClassManager::getRegsBegin() const {
      return Registers.begin();
    }

    std::set<Register*>::const_iterator RegClassManager::getRegsEnd() const {
      return Registers.end();
    }

    std::set<Register*>::const_iterator RegClassManager::getReservedBegin()
      const {
      return ReservedRegisters.begin();
    }

    std::set<Register*>::const_iterator RegClassManager::getReservedEnd() 
      const {
      return ReservedRegisters.end();
    }
    
    std::set<Register*>::const_iterator RegClassManager::getAuxiliarBegin()
      const {
      return AuxiliarRegisters.begin();
    }

    std::set<Register*>::const_iterator RegClassManager::getAuxiliarEnd() 
      const {
      return AuxiliarRegisters.end();
    }

    std::set<Register*>::const_iterator RegClassManager::getCalleeSBegin()
      const {
      return CalleeSaveRegisters.begin();
    }

    std::set<Register*>::const_iterator RegClassManager::getCalleeSEnd() 
      const {
      return CalleeSaveRegisters.end();
    }
    
    std::list<const Register*>* RegClassManager::getCallerSavedRegs() const {
      std::list<const Register*>* Res = new std::list<const Register*>();
      for (std::set<Register*>::const_iterator I = getRegsBegin(),
	   E = getRegsEnd(); I != E; ++I) {
	bool isCS = false;
	for (std::set<Register*>::const_iterator I2 = getCalleeSBegin(),
	     E2 = getCalleeSEnd(); I2 != E2; ++I2) {
	  if ((*I2)->getName() == (*I)->getName())
	    isCS = true;
	}
	/*for (std::set<Register*>::const_iterator I2 = getReservedBegin(),
	     E2 = getReservedEnd(); I2 != E2; ++I2) {
	  if ((*I2)->getName() == (*I)->getName())
	    isCS = true;
	}*/
	if (!isCS)
	  Res->push_back(*I);
      }
      return Res;
    }

    std::list<CallingConvention*>::const_iterator
    RegClassManager::getCCBegin()
      const {
      return CallConvs.begin();
    }

    std::list<CallingConvention*>::const_iterator
    RegClassManager::getCCEnd() 
      const {
      return CallConvs.end();
    }

    // FragmentManager member functions

    FragmentManager::~FragmentManager() {
      for (FragmentManager::Iterator I = FragMap.begin(), E = FragMap.end();
	   I != E; ++I) {
	for (FragmentManager::VecIterator I2 = I->second->begin(), 
	       E2 = I->second->end(); I2 != E2; ++I2) {
	  delete (*I2);
        }
        delete I->second;
      }
    }

    bool FragmentManager::addFragment(const std::string &Name, Tree* Frag) {
      // This key is already occupied
      if (FragMap.find(Name) == FragMap.end())
	FragMap[Name] = new std::vector<Tree*>();

      FragMap[Name]->push_back(Frag);
      return true;
    }

    // This functor is used to rename all leafs of a semantic tree (used 
    // together with ApplyToLeafs<> template in Support.h)
    // The names to use as the new labels are passed as parameter
    // to the constructor (list of strings).
    class RenameLeafsFunctor {
      std::list<std::string>* LeafsName;
    public:
      RenameLeafsFunctor(std::list<std::string>* LeafsName) : 
	LeafsName(LeafsName) {}
      bool operator() (Tree* Opr) {
	if (Opr == NULL) return true;

	Operand* O_and = dynamic_cast<Operand*>(Opr);
	assert (O_and != NULL && "ApplyToLeafs sent us a non-leaf node");

	// Ignore other operands type not register or imm
	if (!dynamic_cast<const RegisterOperand*>(O_and) &&
	    !dynamic_cast<const ImmediateOperand*>(O_and))
	  return true;

	if (LeafsName->empty())
	  return false;

	O_and->changeOperandName(LeafsName->front());
	LeafsName->pop_front();

	return true;
      }
    };

    // Helper class used to analyze a tree and check if it needs
    // to be expanded by fragments.
    // In this functor, we specifically scan for operators with leaf
    // operands represented by fragments. Then, we expand these fragments
    // among the operands of this operator. In the trivial case, there is
    // only one operand expanded and the only the fragment node is substituted
    class ExpandTreeFunctor {
      FragmentManager& Man;
    public:
      ExpandTreeFunctor(FragmentManager& Manager) :
	Man(Manager) {}
      // Return false on error     
      bool operator() (Tree* Opr) {
	Operator* O_ator_ = dynamic_cast<Operator*>(Opr);
	// Not interested in non-operator nodes
	if (O_ator_ == NULL)
	  return true;
	Operator& O_ator = *O_ator_;

	FragOperand* Op = NULL;
	unsigned FragIndex, E = O_ator.getArity();
	// Now scan for all operands looking for a FragOperand
	for (FragIndex = 0; FragIndex != E; ++FragIndex)
	  {	  
	    Op =  dynamic_cast<FragOperand*>(O_ator[FragIndex]);
	    // Not interested in non-frag operands
	    if (Op == NULL)
	      continue;
	    break;
	  }

	// Could not find a fragment to expand
	if (Op == NULL)
	  return true;

	FragmentManager::Iterator Pos = Man.findFrag(Op->getOperandName());
	// We found a fragment, but could not find a definition for it
	// Return false to flag an error
	if (Pos == Man.getEnd()) 
	  return false;	

	std::list<std::string> Names = Op->getParameterList();
	bool ChangeLeafNames = true;
	if (Names.empty())
	  ChangeLeafNames = false;
	  	 
	for (unsigned I = 0, E = Pos->second->size(); I != E; ++I) {
	  Tree* old = O_ator[FragIndex];
	  if (old != NULL)
	    delete old;
	  
	  O_ator[FragIndex] = (*(Pos->second))[I]->clone();
	  if (ChangeLeafNames)
	    if (!ApplyToLeafs<Tree*, Operator*,RenameLeafsFunctor>
		(O_ator[FragIndex], RenameLeafsFunctor(&Names)))
	      return false;	
	}
	return true;
      }
    };
    
    FragmentManager::Iterator FragmentManager::getBegin() {
      return FragMap.begin();
    }
    
    FragmentManager::Iterator FragmentManager::getEnd() {
      return FragMap.end();
    }
    
    FragmentManager::Iterator FragmentManager::findFrag(const std::string &Name)
    {
      return FragMap.find(Name);
    }
    
    bool FragmentManager::expandTree(Tree** Semantic) {      
      // TODO: Expand when the root node is a fragment?
      return ApplyToAllNodes<Tree*, Operator*, ExpandTreeFunctor>
	(*Semantic, ExpandTreeFunctor(*this));
    }

    // RegisterOperand member functions

    RegisterOperand::RegisterOperand(OperandTableManager &Man,
				     const RegisterClass *RegClass,
				     const std::string &OpName):
      Operand(Man, RegClass->getOperandType(), OpName) {
      MyRegClass = RegClass;      
    }

    const RegisterClass* RegisterOperand::getRegisterClass() const {
      return MyRegClass;
    }

    // ImmediateOperand member functions
    ImmediateOperand::ImmediateOperand(OperandTableManager &Man,
				       const OperandType &Type, 
				       const std::string &OpName):
      Operand(Man, Type, OpName) {}

    // PatternManager member functions

    PatternManager::~PatternManager() {
      for (PatternList::iterator I = PatList.begin(), E = PatList.end();
	   I != E; ++I) {
	delete I->TargetImpl;
      }
    }

    void PatternManager::AddPattern(std::string Name, std::string LLVMDAG,
				    const Tree* TargetImpl) {      
      PatList.push_back(PatternElement(Name, LLVMDAG, TargetImpl));
    }
    
    
    /// Generate semantics to find instruction to perform register to
    /// register copy.
    const Tree* PatternManager::genCopyRegToRegPat(OperatorTableManager& OpMan,
						   OperandTableManager& OM,
						   RegClassManager& RegMan,
						   const std::string& DestRC,
						   const std::string& Dest,
						   const std::string& SrcRC,
						   const std::string& Src) {
      RegisterClass *RegClass = RegMan.getRegClass(DestRC);
      RegisterOperand *LHS = new RegisterOperand(OM, RegClass, Dest);
      RegisterClass *RegClassSrc = RegMan.getRegClass(SrcRC);
      RegisterOperand *RHS = new RegisterOperand(OM, RegClassSrc, Src);
      return new AssignOperator(OpMan, LHS, RHS, NULL);
    }
    
    /// Generate semantics to find instruction to perform addition
    /// or subtraction by an immediate
    const Tree* 
    PatternManager::genCopyAddSubImmPat(OperatorTableManager& OpMan,
					OperandTableManager& OM,
					RegClassManager& RegMan,
					bool isAddition,
					const std::string& DestRC,
					const std::string& Dest,
					const std::string& SrcRC,
					const std::string& Src,
					const std::string& Imm) {
      RegisterClass *RegClass = RegMan.getRegClass(DestRC);
      RegisterOperand *LHS = new RegisterOperand(OM, RegClass, Dest);
      OperatorType Ty;
      if (isAddition)
	Ty = OpMan.getType(AddOpStr);
      else
	Ty = OpMan.getType(SubOpStr);
      Operator* RHS = Operator::BuildOperator(OpMan, Ty);
      RegisterClass *SrcRegClass = RegMan.getRegClass(SrcRC);
      (*RHS)[0] = new RegisterOperand(OM, SrcRegClass, Src);
      (*RHS)[1] = new ImmediateOperand(OM, OM.getType("int"), Imm);
      return new AssignOperator(OpMan, LHS, RHS, NULL);
    }
    
    const Tree*
    PatternManager::genNegRegPat(OperatorTableManager& OpMan,
				 OperandTableManager& OM,
				 RegClassManager& RegMan,
				 const std::string& DestRC,
				 const std::string& Dest,
				 const std::string& Imm) {
      RegisterClass *RegClass = RegMan.getRegClass(DestRC);
      RegisterOperand *LHS = new RegisterOperand(OM, RegClass, Dest);
      OperatorType Ty;      
      Ty = OpMan.getType(NegOpStr);
      Operator* NegOp = Operator::BuildOperator(OpMan, Ty);
      (*NegOp)[0] = new ImmediateOperand(OM, OM.getType("int"), Imm);
      return new AssignOperator(OpMan, LHS, NegOp, NULL);
    }

    PatternManager::Iterator PatternManager::begin() {
      return PatList.begin();
    }

    PatternManager::Iterator PatternManager::end() {
      return PatList.end();
    }



  } // End namespace expression

} // End namespace backendgen
