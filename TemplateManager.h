//==-- TemplateManager.h
// Use template files (living in ../template) to produce code to the
// LLVM backend. Manage data necessary to transform template into 
// specific code to the target architecture.

#ifndef TEMPLATEMANAGER_H
#define TEMPLATEMANAGER_H

//==-- includes --==//
#include "InsnSelector/TransformationRules.h"
#include "InsnSelector/Semantic.h"
#include "InsnSelector/Search.h"
#include <cstdlib>
#include <locale>

//==-- Class prototypes --==//

namespace backendgen {

class TemplateManager {
  // Target specific data used when specializing templates for a 
  // given architecture
  std::string ArchName;
  int NumRegs;
  TransformationRules& RuleManager;
  InstrManager& InstructionManager;
  RegClassManager& RegisterClassManager;
  OperandTableManager& OperandTable;
  // Working directory where macro files are created and output
  // is stored.
  const char * WorkingDir;
  
  // Private members
  void CreateM4File();
  std::string generateRegistersDefinitions();
  std::string generateRegisterClassesDefinitions();
  std::string generateCalleeSaveList();
  std::string generateCalleeSaveRegClasses();
  std::string generateReservedRegsList();

  // Private helper functions
  std::string getRegisterClass(Register* Reg);

 public:
  explicit TemplateManager(TransformationRules &TR, InstrManager &IM,
			   RegClassManager& RM, OperandTableManager &OM):
    NumRegs(0), WorkingDir(NULL), RuleManager(TR), InstructionManager(IM),
    RegisterClassManager(RM), OperandTable(OM) {}
  ~TemplateManager() {}

  void SetArchName (const char * name) { 
    std::locale loc;
    ArchName = name; 
    ArchName[0] = std::toupper(ArchName[0], loc);
  }

  void SetNumRegs (int nregs) { NumRegs = nregs; }
  void SetWorkingDir (const char * wdir) { WorkingDir = wdir; }

  void CreateBackendFiles();

  
};

}

#endif
