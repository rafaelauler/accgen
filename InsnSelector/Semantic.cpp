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
	NewType.Type = hash<std::string>(Name);	
	NewType.DataType = NewType.Type;

	// Avoiding collisions
	while (ReverseTypeMap.find(NewType) != ReverseTypeMap.end()) {
	  ++NewType.Type;	  
	}
	
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
	NewType.Type = hash<std::string>(TypeName);

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

    void Operator::setReturnType (const OperandType &OType) {
      ReturnType = OType;
    }

    Operand::Operand (const OperandType &Type) {
      this->Type = Type;
    }

    Operand::Operand () {}

    Constant::Constant (const ConstType Val, const OperandType &Type):
      Operand(Type)
    {
      Value = Val;
    }

    Constant::Constant (const ConstType Val) {
      Value = Val;
    }

    // Register member functions
    Register::Register(const std::string &RegName) {
      Name = RegName;
    }

    void Register::addSubClass(Register *Reg) {
      SubClasses.push_back(Reg);
    }

    const std::string &Register::getName() {
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
    
    const std::string &RegisterClass::getName() const {
      return Name;
    }

    OperandType RegisterClass::getOperandType() const {
      return Type;
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
    }

    bool RegClassManager::addRegClass(RegisterClass *RegClass) {
      return RegClasses.insert(RegClass).second;
    }

    bool RegClassManager::addRegister(Register *Reg) {
      return Registers.insert(Reg).second;
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

    // RegisterOperand member functions

    RegisterOperand::RegisterOperand(const RegisterClass *RegClass,
				     const std::string &OpName):
      Operand(RegClass->getOperandType()) {
      MyRegClass = RegClass;
      OperandName = OpName;
    }
        
    // Instruction member functions    

    // Destructor must deallocate all semantic nodes (expression tree)
    Instruction::~Instruction() {
      for (std::vector<const Tree*>::iterator I = Semantic.begin(),
	     E = Semantic.end(); I != E; ++I)
      {
	delete *I;       
      }
    }

    Instruction::Instruction(const std::string &N): Name(N) {      
    }

    std::string Instruction::getName() const {
      return Name;
    }

    void Instruction::addSemantic(const Tree * Expression) {
      Semantic.push_back(Expression);
    }

    void Instruction::print() {
      std::cout << "Instruction \"" << Name << "\", semantic:\n";
      for (std::vector<const Tree*>::iterator I = Semantic.begin(),
	     E = Semantic.end(); I != E; ++I)
      {
	std::cout << "  ";
	(*I)->print();       
	std::cout << "\n";
      }
      std::cout << "\n\n";
    }

    // InstrManager member functions

    InstrManager::~InstrManager() {
      for (std::vector<Instruction*>::iterator I = Instructions.begin(),
	     E = Instructions.end(); I != E; ++I)
	{
	  delete *I;
	}
    }

    void InstrManager::addInstruction (Instruction *Instr) {
      Instructions.push_back(Instr);
    }

    Instruction *InstrManager::getInstruction(const std::string &Name) {
      Instruction *Pointer = NULL;
      for (std::vector<Instruction*>::iterator I = Instructions.begin(),
	     E = Instructions.end(); I != E; ++I)
	{
	  if ((*I)->getName() == Name) {
	    Pointer = *I;
	    break;
	  }
	} 
      if (Pointer == NULL) {
	Pointer = new Instruction(Name);
	Instructions.push_back(Pointer);
      }
      return Pointer;
    }

    void InstrManager::printAll() {
      std::cout << "Instruction Manager has a total of " << Instructions.size()
		<< " instruction(s).\n";
      std::cout << "==================================\n";
      for (std::vector<Instruction*>::iterator I = Instructions.begin(),
	     E = Instructions.end(); I != E; ++I)
	{
	  (*I)->print();	  
	}
    }

  } // End namespace expression

} // End namespace backendgen
