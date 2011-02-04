//===- AsmProfileGen.h --------------------------------------*- C++ -*-----===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// In this interface, we expose classes responsible for controlling the
// process of assembly profile files generation. Each of these files
// extensively tests a single machine instruction, by changing its operands,
// replicating it several times and embedding it inside a loop.
//
//===----------------------------------------------------------------------===//

#ifndef ASMPROFILEGEN_H
#define ASMPROFILEGEN_H

//==-- includes --==//
#include "InsnSelector/TransformationRules.h"
#include "InsnSelector/Semantic.h"
#include "InsnSelector/Search.h"
#include <string>

//==-- Class prototypes --==//

namespace backendgen {

class AsmProfileGen {
  // Target specific data used when specializing templates for a 
  // given architecture
  std::string ArchName;
  char CommentChar;
  
  TransformationRules& RuleManager;
  InstrManager& InstructionManager;
  RegClassManager& RegisterClassManager;
  OperandTableManager& OperandTable;
  OperatorTableManager& OperatorTable;
  PatternManager& PatMan;

  // Working directory where macro files are created and output
  // is stored.
  const char * WorkingDir;
  
  const static unsigned NUM_INSTRUCTIONS = 100;
  
public:  
  AsmProfileGen(TransformationRules& TR, InstrManager &IR,
		RegClassManager &RCM, OperandTableManager& OTM,
		OperatorTableManager& OtTM, PatternManager& PM):
		RuleManager(TR), InstructionManager(IR),
		RegisterClassManager(RCM), OperandTable(OTM),
		OperatorTable(OtTM), PatMan(PM) {
  }
  
  void SetWorkingDir (const char * wdir) { WorkingDir = wdir; }
  void SetCommentChar (char CommentChar) { this->CommentChar = CommentChar; }
  
  void EmitRandomizedInstruction(InstrIterator Ins, SemanticIterator SI, 
				 std::ostream &O);
  void GenerateAssemblyTest(InstrIterator Ins, std::ostream &O);
  void GenerateFiles();
  void Generate();
};

}


#endif
