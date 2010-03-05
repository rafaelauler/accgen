//==-- TemplateManager.cpp
// Uses template files (living in ../template) to produce code to the
// LLVM backend. Manages data necessary to transform template into 
// specific code to the target architecture.


#include "TemplateManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cassert>

using namespace backendgen;
using namespace backendgen::expression;

// Creates the M4 file that defines macros expansions to be
// performed in template files. These expansions are defines as
// target specific values, so there is no point calling this function
// if all the data needed is not available yet.
void TemplateManager::CreateM4File()
{
  std::string FileName = WorkingDir;
  FileName += "/macro.m4";
  std::ofstream O(FileName.c_str(), std::ios::out);

  // Creates logtable macro
  O << "m4_define(`m4log', `m4_ifelse($1,1,1,m4_ifelse($1,2,1,m4_ifelse($1," <<
    "4,2,m4_ifelse($1,8,3,m4_ifelse($1,16,4,m4_ifelse($1,24,5,m4_ifelse($1," <<
    "32,5,m4_ifelse($1,48,6,m4_ifelse($1,64,6)))))))))')m4_dnl\n";

  // Defines an expansion for each target specific data
  O << "m4_define(`__arch__',`" << ArchName << "')m4_dnl\n"; 
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

inline std::string InferValueType(RegisterClass *RC) {
  if (RC->getOperandType().DataType == IntType) {
    std::stringstream SS;
    SS << "i" << RC->getOperandType().Size;
    return SS.str();
  }

  // Should not reach here!
  assert(0 && "Unknown data type!");

  return std::string("**UNKNOWN**");
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
    SS << "&" << ArchName << "::" << getRegisterClass(*I) << ",";
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
std::string TemplateManager::GenerateInstructionsDefs() {
  std::stringstream SS;

  for (InstrIterator I = InstructionManager.getBegin(), 
	 E = InstructionManager.getEnd(); I != E; ++I) {
    SS << "def " << (*I)->getName() << "\t: " << (*I)->getFormat()->getName();
    SS << "<"
  }
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
