//==-- TemplateManager.cpp
// Uses template files (living in ./template) to produce code to the
// LLVM backend. Manages data necessary to transform template into 
// specific code to the target architecture.

#include "Support.h"
#include "TemplateManager.h"
#include "InsnFormat.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cassert>
#include <ctime>

using namespace backendgen;
using namespace backendgen::expression;

// Creates the M4 file that defines macros expansions to be
// performed in template files. These expansions are defines as
// target specific values, so there is no point calling this function
// if all the data needed is not available yet.
void TemplateManager::CreateM4File()
{
  //  std::locale loc;
  std::string FileName = WorkingDir;
  std::string ArchNameCaps;
  std::transform(ArchName.begin(), ArchName.end(), 
		 std::back_inserter(ArchNameCaps), 
		 std::ptr_fun(toupper));
  FileName += "/macro.m4";
  std::ofstream O(FileName.c_str(), std::ios::out);

  // Creates logtable macro
  O << "m4_define(`m4log', `m4_ifelse($1,1,1,m4_ifelse($1,2,1,m4_ifelse($1," <<
    "4,2,m4_ifelse($1,8,3,m4_ifelse($1,16,4,m4_ifelse($1,24,5,m4_ifelse($1," <<
    "32,5,m4_ifelse($1,48,6,m4_ifelse($1,64,6)))))))))')m4_dnl\n";

  // Defines an expansion for each target specific data
  O << "m4_define(`__arch__',`" << ArchName << "')m4_dnl\n"; 
  O << "m4_define(`__arch_in_caps__',`" << ArchNameCaps << "')m4_dnl\n"; 
  O << "m4_define(`__nregs__',`" << NumRegs << "')m4_dnl\n"; 
  O << "m4_define(`__registers_definitions__',`";
  O << generateRegistersDefinitions() << "')m4_dnl\n";
  O << "m4_define(`__register_classes__',`";
  O << generateRegisterClassesDefinitions() << "')m4_dnl\n";
  O << "m4_define(`__callee_saved_reg_list__',`";
  O << generateCalleeSaveList() << "')m4_dnl\n";
  O << "m4_define(`__callee_saved_reg_classes_list__',`";
  O << generateCalleeSaveRegClasses() << "')m4_dnl\n";
  O << "m4_define(`__reserved_list__',`";
  O << generateReservedRegsList() << "')m4_dnl\n";
  O << "m4_define(`__instructions_definitions__',`";
  O << generateInstructionsDefs() << "')m4_dnl\n";
  O << "m4_define(`__calling_conventions__',`";
  O << generateCallingConventions() << "')m4_dnl\n";
  O << "m4_define(`__data_layout_string__',`";
  O << buildDataLayoutString() << "')m4_dnl\n";
  O << "m4_define(`__set_up_register_classes__',`";
  O << generateRegisterClassesSetup() << "')m4_dnl\n";
  O << "m4_define(`__simple_patterns__',`";
  O << generateSimplePatterns(std::cout) << "')m4_dnl\n";

}

// Inline helper procedure to write a register definition into tablegen
// register file (XXXRegisterInfo.td)
inline void DefineRegister(std::stringstream &SS, Register *R,
			   unsigned Counter) {
  SS << "  def " << R->getName() << " : AReg<" << Counter;
  SS << ", \"" << R->getName() << "\", [], [";
  // Has subregisters
  if (R->getSubsBegin() != R->getSubsEnd()) {
    for (std::list<Register*>::const_iterator I = R->getSubsBegin(),
	   E = R->getSubsEnd(); I != E; ++I) {
      if (I != R->getSubsBegin())
	SS << ",";
      SS << (*I)->getName();
    }   
  } 
  SS << "]>;\n";
}

// Generates the XXXRegisterInfo.td registers definitions
std::string TemplateManager::generateRegistersDefinitions() {  
  std::stringstream SS;
  unsigned Counter = 0;
  std::set<Register*> DefinedRegisters;

  for (std::set<Register*>::const_iterator 
	 I = RegisterClassManager.getRegsBegin(), 
	 E = RegisterClassManager.getRegsEnd(); I != E; ++I) {
    // For each subregister , define it first if it is not defined already
    for (std::list<Register*>::const_iterator
	   I2 = (*I)->getSubsBegin(), E2 = (*I)->getSubsEnd();
	   I2 != E2; ++I2) {
      if (DefinedRegisters.find(*I2) == DefinedRegisters.end()) {
	DefineRegister(SS, *I2, Counter);
	++Counter;
	DefinedRegisters.insert(*I2);
      }
    }
    DefineRegister(SS, *I, Counter);
    ++Counter;
    DefinedRegisters.insert(*I);
  }

  return SS.str();
}

// This procedure build the LLVM data layout string used to initialize
// a DataLayout class living inside TargetMachine.
std::string TemplateManager::buildDataLayoutString() {
  std::stringstream SS;

  // Endianess
  if (IsBigEndian)
    SS << "E";
  else
    SS << "e";
  // Now for pointer information
  // XXX: Hardcoded prefered alignment as the same as pointer size.
  SS << "-p:" << WordSize << ":" << WordSize;

  return SS.str();
}

// Generates the list used in XXXRegisterInfo.cpp
std::string TemplateManager::generateCalleeSaveList() {
  std::stringstream SS;

  SS << "static const unsigned CalleeSavedRegs[] = {";

  for (std::set<Register*>::const_iterator 
	 I = RegisterClassManager.getCalleeSBegin(),
	 E = RegisterClassManager.getCalleeSEnd(); I != E; ++I) {
    SS << ArchName << "::" << (*I)->getName() << ", ";
  }
  SS << "0};";
  return SS.str();
}

// Helper function for generateCalleeSaveRegClasses()
inline std::string TemplateManager::getRegisterClass(Register* Reg) {
  RegisterClass* RC = RegisterClassManager.getRegRegClass(Reg);
  assert (RC != NULL && "Register has no associated Register Class");
  return RC->getName();
}

// Generates list used in XXXRegisterInfo.cpp
std::string TemplateManager::generateCalleeSaveRegClasses() {
  std::stringstream SS;

  SS << "static const TargetRegisterClass * const CalleeSavedRC[] = {";

  for (std::set<Register*>::const_iterator 
	 I = RegisterClassManager.getCalleeSBegin(),
	 E = RegisterClassManager.getCalleeSEnd(); I != E; ++I) {
    SS << "&" << ArchName << "::" << getRegisterClass(*I) << "RegClass" << ",";
  }
  SS << "0};";
  return SS.str();
}

// Generates a reserved regs list code used in XXXRegisterInfo.cpp
std::string TemplateManager::generateReservedRegsList() {
  std::stringstream SS;

  for (std::set<Register*>::const_iterator
	 I = RegisterClassManager.getReservedBegin(),
	 E = RegisterClassManager.getReservedEnd(); I != E; ++I) {
    SS << "  Reserved.set(" << ArchName << "::" << (*I)->getName() << ");\n";
  }
  return SS.str();
}

// Generates the register classes set up in ISelLowering.cpp
std::string TemplateManager::generateRegisterClassesSetup() {
  std::stringstream SS;

  for (std::set<RegisterClass*>::const_iterator
	 I = RegisterClassManager.getBegin(),
	 E = RegisterClassManager.getEnd(); I != E; ++I) {
    SS << "  addRegisterClass(MVT::" << InferValueType(*I) << ", "
       << ArchName << "::" << (*I)->getName() << "RegisterClass);\n";
  }

  return SS.str();
}

// Generates the XXXRegisterInfo.td register classes
std::string TemplateManager::generateRegisterClassesDefinitions() {
  std::stringstream SS;

  for (std::set<RegisterClass*>::const_iterator
	 I = RegisterClassManager.getBegin(),
	 E = RegisterClassManager.getEnd(); I != E; ++I) {
    SS << "def " << (*I)->getName() << ": RegisterClass<\"" << ArchName;
    // XXX: Support different alignments! (Hardcoded as 32)
    SS << "\", [" << InferValueType(*I) << "], 32, [";
    // Iterates through all registers in this class
    for (std::set<Register*>::const_iterator R = (*I)->getBegin(),
	   E2 = (*I)->getEnd(); R != E2; ++R) {
      if (R != (*I)->getBegin())
	SS << ",";
      SS << (*R)->getName();     
    }
    SS << "]>;\n\n";
    // TODO: Support for infinite copy cost class of registers (registers
    // that shouldn't be copied
    // TODO: Support for reserved registers types that shouldn't be
    // used in the register allocator (see MIPS example).
  }

  return SS.str();
}

// Generate tablegen instructions definitions for XXXInstrInfo.td
std::string TemplateManager::generateInstructionsDefs() {
  std::stringstream SS;

  unsigned CurInsVersion = 0;
  for (InstrIterator I = InstructionManager.getBegin(), 
	 E = InstructionManager.getEnd(), Prev = InstructionManager.getEnd();
       I != E; Prev = I++) {
    if (Prev != E && (*Prev)->getName() != (*I)->getName())
      CurInsVersion = 0;
    // Define the instruction's LLVM name, a string tag used whenever we
    // reference this instruction in LLVM generated code.
    {
      std::stringstream SStmp;
      SStmp << (*I)->getName() << "_" << ++CurInsVersion;
      std::string LLVMName = SStmp.str();
      (*I)->setLLVMName(LLVMName);
    }
    // Here we print defs and uses for this instruction
    CnstOperandsList *Defs = (*I)->getDefs(), *Uses = (*I)->getUses();
    if (Defs->size() > 0 || Uses->size() > 0) {
      SS << "let ";
      if (Defs->size() > 0) {
	SS << "Defs = [";
	for (CnstOperandsList::const_iterator I2 = Defs->begin(), 
	       E2 = Defs->end(); I2 != E2; ++I2) {
	  if (I2 != Defs->begin())
	    SS << ",";	  
	  SS << (*I2)->getOperandName();      
	}
	SS << "]";
	if (Uses->size() > 0) SS << ", ";
      }
      if (Uses->size() > 0) {
	SS << "Uses = [";
	for (CnstOperandsList::const_iterator I2 = Uses->begin(), 
	       E2 = Uses->end(); I2 != E2; ++I2) {
	  if (I2 != Uses->begin())
	    SS << ",";	  
	  SS << (*I2)->getOperandName();      
	}
	SS << "]";
      }
      SS << " in\n";
    }
    delete Defs;
    delete Uses;
    // No we begin printing the instruction definition per se.
    SS << "def " << (*I)->getLLVMName() << "\t: " << (*I)->getFormat()
      ->getName();
    SS << "<(outs ";
    CnstOperandsList *Ins = (*I)->getIns(), *Outs = (*I)->getOuts();
    // Prints all defined operands (outs)
    for (CnstOperandsList::const_iterator I2 = Outs->begin(),
	   E2 = Outs->end(); I2 != E2; ++I2) {
      if (I2 != Outs->begin())
	SS << ",";
      const RegisterOperand* RO = dynamic_cast<const RegisterOperand*>(*I2);
      // If it is not a register operand, it must be a memory reference
      // FIXME: Enforce this
      if (RO != NULL) {
	SS << RO->getRegisterClass()->getName();
	SS << ":$" << RO->getOperandName();
      } 
    }
    delete Outs;
    SS << "), (ins ";
    // Prints all used operands (ins)
    int DummyIndex = 1;
    for (std::list<const Operand*>::const_iterator I2 = Ins->begin(),
	   E2 = Ins->end(); I2 != E2; ++I2) {
      if (I2 != Ins->begin())
	SS << ",";
      const RegisterOperand* RO = dynamic_cast<const RegisterOperand*>(*I2);
      // If not a register, treat it as immediate
      if (RO == NULL) {
	// Type 0 is a reserved value for "dummy operand", meaning we don't
	// know what kind this operand is (it is never mentioned in the 
	// semantic tree).
	if ((*I2)->getType() == 0) {
	  // LLVM Operand "tgliteral" is tablegen defined and reserved
	  // for dummies
	  SS << "tgliteral:$dummy" << DummyIndex++;
	} else {
	  SS << InferValueType(*I2) << "imm";
	  SS << ":$" << (*I2)->getOperandName();
	}
	continue;
      } 
      SS << RO->getRegisterClass()->getName();
      SS << ":$" << RO->getOperandName();
    }
    delete Ins;
    SS << "), \"" << (*I)->parseOperandsFmts() << "\", []>;\n";
  }

  return SS.str();
}

std::string TemplateManager::generateCallingConventions() {
  std::stringstream SS;
  
  // First generate all return conventions
  SS << "def RetCC_" << ArchName << ": CallingConv <[\n";
  bool First = true;
  for (std::list<CallingConvention*>::const_iterator 
	 I = RegisterClassManager.getCCBegin(), 
	 E = RegisterClassManager.getCCEnd(); I != E; ++I) {
    CallingConvention* CC = *I;
    if (!(CC->IsReturnConvention))
      continue; // Not return convention
    if (First) 
      First = false;
    else
      SS << ",";
    if (CC->Type.Type != 0)
      SS << "  CCIfType<[" << InferValueType(&(CC->Type)) << "], ";
    if (CC->UseStack)
      SS << "CCAssignToStack<" << CC->StackSize << "," << CC->StackAlign << ">";
    else {
      SS << "CCAssignToReg<[";
      for (std::list<const Register*>::const_iterator I2 = CC->getBegin(),
	     E2 = CC->getEnd(); I2 != E2; ++I2) {
	if (I2 != CC->getBegin())
	  SS << ",";
	SS << (*I2)->getName();
      }
      SS << "]>";      
    }
    if (CC->Type.Type != 0)
      SS << ">";
    SS << "\n";
  }
  SS << "]>;\n\n";
  
  // Now generate all calling conventions
  First = true;
  SS << "def CC_" << ArchName << ": CallingConv <[\n";
  for (std::list<CallingConvention*>::const_iterator 
	 I = RegisterClassManager.getCCBegin(), 
	 E = RegisterClassManager.getCCEnd(); I != E; ++I) {
    CallingConvention* CC = *I;
    if (CC->IsReturnConvention)
      continue; // Not calling convention
    if (First) 
      First = false;
    else
      SS << ",";
    if (CC->Type.Type != 0)
      SS << "  CCIfType<[" << InferValueType(&(CC->Type)) << "], ";
    if (CC->UseStack)
      SS << "CCAssignToStack<" << CC->StackSize << "," << CC->StackAlign << ">";
    else {
      SS << "CCAssignToReg<[";
      for (std::list<const Register*>::const_iterator I2 = CC->getBegin(),
	     E2 = CC->getEnd(); I2 != E2; ++I2) {
	if (I2 != CC->getBegin())
	  SS << ",";
	SS << (*I2)->getName();
      }
      SS << "]>";      
    }
    if (CC->Type.Type != 0)
      SS << ">";
    SS << "\n";
  }
  SS << "]>;\n\n";
  return SS.str();
}

SearchResult* TemplateManager::FindImplementation(const expression::Tree *Exp,
						  std::ostream &Log) {
  Search S(RuleManager, InstructionManager);
  unsigned SearchDepth = 5;
  SearchResult *R = NULL;
  // Increasing search depth loop - first try with low depth to speed up
  // easy matches
  while (R == NULL || R->Instructions->size() == 0) {
    if (SearchDepth == 10)
      break;
    Log << "  Trying search with depth " << SearchDepth << "\n";
    S.setMaxDepth(SearchDepth++);
    if (R != NULL)
      delete R;
    R = S(Exp, 0, NULL);
  }
  // Detecting failures
  if (R == NULL) {
    Log << "  Not found!\n";
    return NULL;
  } else if (R->Instructions->size() == 0) {
    Log << "  Not found!\n";
    delete R;
    return NULL;
  }
  
  return R;                 
}

// In this member function, a LLVM DAG String (that is, a LLVM SelectionDAG
// to be matched) is postprocessed and its node types may be changed
// according to the target implementation found for this particular DAG.
// Here is the reasoning: If the operand is a Register, then its type
// is copied to the LLVM string, because the LLVM backend knows and recognizes
// with a specific type every register class. But in other cases, the
// opposite happens: the LLVM string defines a type that is copied to the
// pattern implementation node types, because only the LLVM string knows
// better the exact SDNode types used in the LLVM backend.
std::string 
TemplateManager::PostprocessLLVMDAGString(const std::string &S, SDNode *DAG) {
  std::string Result(S);
  // Traverse 
  std::list<SDNode*> Queue;
  Queue.push_back(DAG);
  while (Queue.size() > 0) {
    SDNode* Element = Queue.front();
    Queue.pop_front();
    // If not leaf, simply add its children without further processing
    if (Element->NumOperands != 0) {
      for (unsigned I = 0, E = Element->NumOperands; I < E; ++I) {
	Queue.push_back(Element->ops[I]);	
      }
      continue;
    }
    // If dummy
    if (Element->IsLiteral)
      continue;
    // If leaf and not dummy, check the name and, if this name can
    // be found in the LLVM DAG string, change the LLVM node to its type
    // only if it is of register type (TypeName should be empty, otherwise).
    // In case TypeName is empty, do the reverse substitution.
    assert(Element->OpName != NULL && "Leaf SDNode type must have a name");
    std::string::size_type idx = Result.find(*Element->OpName);
    if (idx == std::string::npos)
	continue;
    std::string::size_type idx2 = Result.rfind(" ", idx);
    assert(idx2 != 0 && idx2 != std::string::npos && 
	   "Malformed LLVM DAG String");    
    if (Element->TypeName.size() > 0) {
	Result.replace(idx2+1, idx-3-idx2, Element->TypeName);
	continue;
    } 
    // Element is not a register operand. So we define its typename
    // with the string of the LLVM DAG.
    Element->TypeName = Result.substr(idx2+1, idx-3-idx2);    
  }
  return Result;
}


// Here we must find the implementation of several simple patterns. For that
// we use the search algorithm.
std::string TemplateManager::generateSimplePatterns(std::ostream &Log) {
  std::stringstream SS;
  unsigned count = 0;
  time_t start, end;
  start = std::time(0);
  Log << "Pattern implementation inference will start now. This may take"
      << " several\nminutes.\n\n";
  for (PatternManager::Iterator I = PatMan.begin(), E = PatMan.end();
       I != E; ++I) {
    count ++;
    Log << "Now finding implementation for : " << I->Name << "\n";
    SearchResult *SR = FindImplementation(I->TargetImpl, Log);
    if (SR == NULL) {
      std::cerr << "Failed: Could not find implementation for pattern " <<
	I->Name << "\n";
      abort();
    }
    SDNode* DAG = PatTrans.generateInsnsDAG(SR);    
    SS << "def " << I->Name << " : Pat<" 
       << PostprocessLLVMDAGString(I->LLVMDAG, DAG) << ",\n";
    DAG->Print(SS);
    std::cerr << PatTrans.generateEmitCode(SR, I->LLVMDAG) << std::endl;
    SS << ">;\n\n";
    delete DAG;
  }
  end = std::time(0);
  Log << count << " pattern(s) implemented successfully in " << 
    std::difftime(end,start) << " second(s).\n";

  return SS.str();
}

// Creates LLVM backend files based on template files feeded with
// target specific data.
void TemplateManager::CreateBackendFiles()
{
  int ret;
  std::string MacroFileName = WorkingDir;
  std::string RegisterInfoIn = "./template/XXXRegisterInfo.td";
  std::string RegisterInfoOut = WorkingDir;
  std::string RegisterInfo2In = "./template/XXXRegisterInfo.cpp";
  std::string RegisterInfo2Out = WorkingDir;
  std::string RegisterInfo3In = "./template/XXXRegisterInfo.h";
  std::string RegisterInfo3Out = WorkingDir;
  std::string InstrInfoIn = "./template/XXXInstrInfo.td";
  std::string InstrInfoOut = WorkingDir;
  std::string InstrInfo2In = "./template/XXXInstrInfo.cpp";
  std::string InstrInfo2Out = WorkingDir;
  std::string InstrInfo3In = "./template/XXXInstrInfo.h";
  std::string InstrInfo3Out = WorkingDir;
  std::string ISelDAGToDAGIn = "./template/XXXISelDAGToDAG.cpp";
  std::string ISelDAGToDAGOut = WorkingDir;
  std::string ISelLoweringIn = "./template/XXXISelLowering.cpp";
  std::string ISelLoweringOut = WorkingDir;
  std::string ISelLowering2In = "./template/XXXISelLowering.h";
  std::string ISelLowering2Out = WorkingDir;
  std::string TargetIn = "./template/XXX.td";
  std::string TargetOut = WorkingDir;
  std::string Target2In = "./template/XXX.h";
  std::string Target2Out = WorkingDir;
  std::string SubtargetIn = "./template/XXXSubtarget.cpp";
  std::string SubtargetOut = WorkingDir;
  std::string Subtarget2In = "./template/XXXSubtarget.h";
  std::string Subtarget2Out = WorkingDir;
  std::string TargetMachineIn = "./template/XXXTargetMachine.cpp";
  std::string TargetMachineOut = WorkingDir;
  std::string TargetMachine2In = "./template/XXXTargetMachine.h";
  std::string TargetMachine2Out = WorkingDir;
  std::string TargetAsmInfoIn = "./template/XXXTargetAsmInfo.cpp";
  std::string TargetAsmInfoOut = WorkingDir;
  std::string TargetAsmInfo2In = "./template/XXXTargetAsmInfo.h";
  std::string TargetAsmInfo2Out = WorkingDir;
  std::string AsmPrinterIn = "./template/XXXAsmPrinter.cpp";
  std::string AsmPrinterOut = WorkingDir;
  std::string CallingConvIn = "./template/XXXCallingConv.td";
  std::string CallingConvOut = WorkingDir;
  std::string MachineFunctionIn = "./template/XXXMachineFunction.h";
  std::string MachineFunctionOut = WorkingDir;
  std::string CMakeListsIn = "./template/CMakeLists.txt";
  std::string CMakeListsOut = WorkingDir;
  std::string MakefileIn = "./template/Makefile";
  std::string MakefileOut = WorkingDir; 

  MacroFileName += "/macro.m4";
  RegisterInfoOut += "/";
  RegisterInfoOut += ArchName;
  RegisterInfoOut += "RegisterInfo.td";
  RegisterInfo2Out += "/";
  RegisterInfo2Out += ArchName;
  RegisterInfo2Out += "RegisterInfo.cpp";
  RegisterInfo3Out += "/";
  RegisterInfo3Out += ArchName;
  RegisterInfo3Out += "RegisterInfo.h";
  InstrInfoOut += "/";
  InstrInfoOut += ArchName;
  InstrInfoOut += "InstrInfo.td";
  InstrInfo2Out += "/";
  InstrInfo2Out += ArchName;
  InstrInfo2Out += "InstrInfo.cpp";
  InstrInfo3Out += "/";
  InstrInfo3Out += ArchName;
  InstrInfo3Out += "InstrInfo.h";
  ISelDAGToDAGOut += "/";
  ISelDAGToDAGOut += ArchName;
  ISelDAGToDAGOut += "ISelDAGToDAG.cpp";
  ISelLoweringOut += "/";
  ISelLoweringOut += ArchName;
  ISelLoweringOut += "ISelLowering.cpp";
  ISelLowering2Out += "/";
  ISelLowering2Out += ArchName;
  ISelLowering2Out += "ISelLowering.h";
  CallingConvOut += "/";
  CallingConvOut += ArchName;
  CallingConvOut += "CallingConv.td";
  TargetOut += "/";
  TargetOut += ArchName;
  TargetOut += ".td";
  Target2Out += "/";
  Target2Out += ArchName;
  Target2Out += ".h";
  SubtargetOut += "/";
  SubtargetOut += ArchName;
  SubtargetOut += "Subtarget.cpp";
  Subtarget2Out += "/";
  Subtarget2Out += ArchName;
  Subtarget2Out += "Subtarget.h";
  TargetMachineOut += "/";
  TargetMachineOut += ArchName;
  TargetMachineOut += "TargetMachine.cpp";
  TargetMachine2Out += "/";
  TargetMachine2Out += ArchName;
  TargetMachine2Out += "TargetMachine.h";
  TargetAsmInfoOut += "/";
  TargetAsmInfoOut += ArchName;
  TargetAsmInfoOut += "TargetAsmInfo.cpp";
  TargetAsmInfo2Out += "/";
  TargetAsmInfo2Out += ArchName;
  TargetAsmInfo2Out += "TargetAsmInfo.h";
  AsmPrinterOut += "/";
  AsmPrinterOut += ArchName;
  AsmPrinterOut += "AsmPrinter.cpp";
  MachineFunctionOut += "/";
  MachineFunctionOut += ArchName;
  MachineFunctionOut += "MachineFunction.h";

  CMakeListsOut += "/CMakeLists.txt";
  MakefileOut += "/Makefile";

  // First creates our macro file to insert target specific data
  // into templates
  CreateM4File();

  // Creates XXXRegisterInfo.td, h, cpp files
  ret = system(("m4 -P " + MacroFileName + " " + RegisterInfoIn +
		" > " + RegisterInfoOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXRegisterInfo.td\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + RegisterInfo2In +
		" > " + RegisterInfo2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXRegisterInfo.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + RegisterInfo3In +
		" > " + RegisterInfo3Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXRegisterInfo.h\n";
    exit(1);
  }

  // Creates XXXInstrInfo.td, h, cpp files
  ret = system(("m4 -P " + MacroFileName + " " + InstrInfoIn +
		" > " + InstrInfoOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXInstrInfo.td\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + InstrInfo2In +
		" > " + InstrInfo2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXInstrInfo.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + InstrInfo3In +
		" > " + InstrInfo3Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXInstrInfo.h\n";
    exit(1);
  }
  // Creates XXXISelDAGToDAG.cpp file
  ret = system(("m4 -P " + MacroFileName + " " + ISelDAGToDAGIn +
		" > " + ISelDAGToDAGOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXISelDAGToDAG.cpp\n";
    exit(1);
  }

  // Creates XXXISelLowering.cpp and h files
  ret = system(("m4 -P " + MacroFileName + " " + ISelLoweringIn +
		" > " + ISelLoweringOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXISelLowering.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + ISelLowering2In +
		" > " + ISelLowering2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXISelLowering.h\n";
    exit(1);
  }

  // Creates XXXCallingConv.td file
  ret = system(("m4 -P " + MacroFileName + " " + CallingConvIn +
		" > " + CallingConvOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXCallingConv.td\n";
    exit(1);
  }

  // Creates XXX.td and files
  ret = system(("m4 -P " + MacroFileName + " " + TargetIn +
		" > " + TargetOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXX.td\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + Target2In +
		" > " + Target2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXX.h\n";
    exit(1);
  }

  // Creates XXXSubtarget.cpp and h files
  ret = system(("m4 -P " + MacroFileName + " " + SubtargetIn +
		" > " + SubtargetOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXSubtarget.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + Subtarget2In +
		" > " + Subtarget2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXSubtarget.h\n";
    exit(1);
  }

  // Creates XXXTargetMachine.cpp and h files
  ret = system(("m4 -P " + MacroFileName + " " + TargetMachineIn +
		" > " + TargetMachineOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXTargetMachine.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + TargetMachine2In +
		" > " + TargetMachine2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXTargetMachine.h\n";
    exit(1);
  }

  // Creates XXXTargetAsmInfo.cpp and h files
  ret = system(("m4 -P " + MacroFileName + " " + TargetAsmInfoIn +
		" > " + TargetAsmInfoOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXTargetAsmInfo.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + TargetAsmInfo2In +
		" > " + TargetAsmInfo2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXTargetAsmInfo.h\n";
    exit(1);
  }

  // Creates XXXAsmPrinter.cpp file
  ret = system(("m4 -P " + MacroFileName + " " + AsmPrinterIn +
		" > " + AsmPrinterOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXAsmPrinter.cpp\n";
    exit(1);
  }

  // Creates XXXMachineFunction.h file
  ret = system(("m4 -P " + MacroFileName + " " + MachineFunctionIn +
		" > " + MachineFunctionOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXMachineFunction.h\n";
    exit(1);
  }

  // Copy CMakeLists and Makefile
  ret = system(("m4 -P " + MacroFileName + " " + CMakeListsIn +
		" > " + CMakeListsOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo CMakeLists.txt\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + MakefileIn +
		" > " + MakefileOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo Makefile\n";
    exit(1);
  }

  // Success
  remove(MacroFileName.c_str());
}
