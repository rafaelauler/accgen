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
#include "PatternTranslator.h"
#include <cstdlib>
#include <locale>

//==-- Class prototypes --==//

namespace backendgen {

class TemplateManager {
  // Target specific data used when specializing templates for a 
  // given architecture
  std::string ArchName;
  int NumRegs;
  bool IsBigEndian;
  unsigned WordSize;
  TransformationRules& RuleManager;
  InstrManager& InstructionManager;
  RegClassManager& RegisterClassManager;
  OperandTableManager& OperandTable;
  PatternManager& PatMan;
  PatternTranslator PatTrans;
  // Working directory where macro files are created and output
  // is stored.
  const char * WorkingDir;
  
  // Private members
  void CreateM4File();
  std::string generateRegistersDefinitions();
  std::string generateRegisterClassesSetup();
  std::string generateRegisterClassesDefinitions();
  std::string generateCalleeSaveList();
  std::string generateCalleeSaveRegClasses();
  std::string generateReservedRegsList();
  std::string generateInstructionsDefs();
  std::string generateCallingConventions();
  SearchResult* FindImplementation(const expression::Tree *Exp,
				   std::ostream &Log);
  std::string PostprocessLLVMDAGString(const std::string &S, SDNode *DAG);
  std::string generateSimplePatterns(std::ostream &Log);
  std::string buildDataLayoutString();

  // Private helper functions
  std::string getRegisterClass(Register* Reg);

 public:
  explicit TemplateManager(TransformationRules &TR, InstrManager &IM,
			   RegClassManager& RM, OperandTableManager &OM,
			   PatternManager& PM):
  NumRegs(0), IsBigEndian(true), WordSize(32), RuleManager(TR),
    InstructionManager(IM), RegisterClassManager(RM), OperandTable(OM),
    PatMan(PM), PatTrans(OM), WorkingDir(NULL) {}

  ~TemplateManager() {}

  void SetArchName (const char * name) { 
    std::locale loc;
    ArchName = name; 
    ArchName[0] = std::toupper(ArchName[0], loc);
  }

  void SetNumRegs (int nregs) { NumRegs = nregs; }
  void SetWorkingDir (const char * wdir) { WorkingDir = wdir; }
  void SetIsBigEndian (bool val) { IsBigEndian = val; }
  void SetWordSize(unsigned val) { WordSize = val; }

  void CreateBackendFiles();

  
};

}

#endif
