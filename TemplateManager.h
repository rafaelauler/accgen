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
  char CommentChar;
  char TypeCharSpecifier;
  int NumRegs;
  bool IsBigEndian;
  unsigned WordSize;
  TransformationRules& RuleManager;
  InstrManager& InstructionManager;
  RegClassManager& RegisterClassManager;
  OperandTableManager& OperandTable;
  OperatorTableManager& OperatorTable;
  PatternManager& PatMan;
  PatternTranslator PatTrans;
  // Working directory where macro files are created and output
  // is stored.
  const char * WorkingDir;
  LiteralMap LMap;
  std::list<const Register*> AuxiliarRegs;
  
  typedef std::pair<const RegisterClass*, const RegisterClass*> RCPair;
  typedef std::list<std::pair<RCPair,
      SearchResult *> > MoveListTy;
  // Important pattern inferece results, used more than once
  // in the backend generation process.
  struct _InferenceResults {
    // StoreToStackSlotSR is a pattern with operands val and src, used to store
    // reg "src" into frameindex "val"
    SearchResult * StoreToStackSlotSR;
    // LoadFromStackSlotSR is a pattern with operands addr, used to load from
    // frameindex "addr" to a register "dest".
    SearchResult * LoadFromStackSlotSR;
    // A list of patterns of move instructions, between each pair of possible
    // register class
    MoveListTy MoveRegToRegList;
  } InferenceResults;
  
  // Private members
  void CreateM4File();
  std::string generateRegistersDefinitions();
  std::string generateRegisterClassesSetup();
  std::string generateRegisterClassesDefinitions();
  std::string generateCalleeSaveList();
  std::string generateCalleeSaveRegClasses();
  std::string generateReservedRegsList();
  std::string generateInstructionsDefs();
  std::string generateInsnSizeTable();
  std::string generateCallingConventions();
  std::string generatePrintLiteral();
  std::string generateStoreRegToStackSlot();
  std::string generateIsStoreToStackSlot();
  std::string generateLoadRegFromStackSlot();  
  std::string generateIsLoadFromStackSlot();
  std::string generateEliminateCallFramePseudo(std::ostream &Log,
					       bool isPositive);
  std::string generateEmitPrologue(std::ostream &Log);
  std::string generateEmitEpilogue(std::ostream &Log);
  std::string generateCopyRegPatterns(std::ostream &Log);
  std::string generateIsMove();
  SearchResult* FindImplementation(const expression::Tree *Exp,
				   std::ostream &Log);
  std::string PostprocessLLVMDAGString(const std::string &S, SDNode *DAG);
  std::string generateReturnLowering();
  void generateSimplePatterns(std::ostream &Log, std::string **EmitFunctions,
			      std::string **SwitchCode, 
			      std::string **EmitHeaders);
  std::string buildDataLayoutString();

  // Private helper functions
  std::string getRegisterClass(Register* Reg);

 public:
  explicit TemplateManager(TransformationRules &TR, InstrManager &IM,
			   RegClassManager& RM, OperandTableManager &OM,
			   OperatorTableManager &ORM,
			   PatternManager& PM):
  NumRegs(0), IsBigEndian(true), WordSize(32), RuleManager(TR),
    InstructionManager(IM), RegisterClassManager(RM), OperandTable(OM),
    OperatorTable(ORM), PatMan(PM), PatTrans(OM), WorkingDir(NULL) {
      CommentChar = '#';
      TypeCharSpecifier = '@';
      InferenceResults.StoreToStackSlotSR = NULL;
      for (std::set<Register*>::const_iterator 
	I = RegisterClassManager.getAuxiliarBegin(),
	E = RegisterClassManager.getAuxiliarEnd(); I != E; ++I) {
	AuxiliarRegs.push_back(*I);
      }
    }

  ~TemplateManager() {
    if (InferenceResults.StoreToStackSlotSR != NULL)
      delete InferenceResults.StoreToStackSlotSR;
    if (InferenceResults.LoadFromStackSlotSR != NULL)
      delete InferenceResults.LoadFromStackSlotSR;
    for (MoveListTy::const_iterator I = InferenceResults.MoveRegToRegList
         .begin(), E = InferenceResults.MoveRegToRegList.end(); I != E; ++I) {
      if (I->second != NULL)
	delete I->second;
    }
  }

  void SetArchName (const char * name) { 
    std::locale loc;
    ArchName = name; 
    ArchName[0] = std::toupper(ArchName[0], loc);
  }
  
  void SetCommentChar (char CommentChar) {
    this->CommentChar = CommentChar;
    if (CommentChar == '@')
      TypeCharSpecifier = '%';
    else
      TypeCharSpecifier = '@';
  }

  void SetNumRegs (int nregs) { NumRegs = nregs; }
  void SetWorkingDir (const char * wdir) { WorkingDir = wdir; }
  void SetIsBigEndian (bool val) { IsBigEndian = val; }
  void SetWordSize(unsigned val) { WordSize = val; }

  void CreateBackendFiles();

  
};

}

#endif
