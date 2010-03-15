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
    unsigned int hash (const T &Name) {
      unsigned long int Hash = 0;
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
    
    OperandType OperandTableManager::getType (const std::string &Name) {    
      
      if (TypeMap.find(Name) == TypeMap.end()) {
	OperandType NewType;

	NewType.Size = 32;

	// Try to recognize known types and use the adequate code
	if (Name == IntTypeStr)
	  NewType.Type = IntType;
	else {
	  NewType.Type = hash<std::string>(Name);	
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
	else if (TypeName == DecompOpStr)
	  NewType.Type = DecompOp;
	else if (TypeName == IfOpStr)
	  NewType.Type = IfOp;
	else if (TypeName == AssignOpStr)
	  NewType.Type = AssignOp;
	else { // User defined operator
	  NewType.Type = hash<std::string>(TypeName);
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
      return new Operator(OpType);
    }

    void Operator::setReturnType (const OperandType &OType) {
      ReturnType = OType;
    }

    // Operand member functions 

    Operand::Operand (const OperandType &Type, const std::string &OpName) {
      this->Type = Type;
      OperandName = OpName;
    }

    // Contant member functions

    Constant::Constant (const ConstType Val, const OperandType &Type):
      Operand(Type, "C")
    {
      Value = Val;
    }

    // Register member functions
    Register::Register(const std::string &RegName) {
      Name = RegName;
    }

    void Register::addSubClass(Register *Reg) {
      SubClasses.push_back(Reg);
    }

    std::list<Register*>::const_iterator Register::getSubsBegin() const {
      return SubClasses.begin();
    }
    
    std::list<Register*>::const_iterator Register::getSubsEnd() const {
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

    bool RegisterClass::hasRegister(Register *Reg) {
      return (Registers.find(Reg) != Registers.end());
    }
    
    const std::string &RegisterClass::getName() const {
      return Name;
    }

    OperandType RegisterClass::getOperandType() const {
      return Type;
    }
    
    std::set<Register*>::const_iterator RegisterClass::getBegin() const {
      return Registers.begin();
    }
    
    std::set<Register*>::const_iterator RegisterClass::getEnd() const {
      return Registers.end();
    }


    // RegClassManager member functions

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

    // Adds one register to the list of reserved registers. Same restrictions
    // above apply.
    bool RegClassManager::addReservedRegister(Register *Reg) {
      return ReservedRegisters.insert(Reg).second;
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

    RegisterClass *RegClassManager::getRegRegClass(Register* Reg) {
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

    std::set<Register*>::const_iterator RegClassManager::getCalleeSBegin()
      const {
      return CalleeSaveRegisters.begin();
    }

    std::set<Register*>::const_iterator RegClassManager::getCalleeSEnd() 
      const {
      return CalleeSaveRegisters.end();
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

    RegisterOperand::RegisterOperand(const RegisterClass *RegClass,
				     const std::string &OpName):
      Operand(RegClass->getOperandType(), OpName) {
      MyRegClass = RegClass;      
    }

    const RegisterClass* RegisterOperand::getRegisterClass() const {
      return MyRegClass;
    }

    // ImmediateOperand member functions
    ImmediateOperand::ImmediateOperand(const OperandType &Type, 
				       const std::string &OpName):
      Operand(Type, OpName) {}
        


  } // End namespace expression

} // End namespace backendgen
