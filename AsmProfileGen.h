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
#include "PatternTranslator.h"
#include <string>

//==-- Class prototypes --==//

using namespace backendgen::expression;

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

	SearchResult* PrologImpl;
	SearchResult* IncRegImpl;
	SearchResult* EpilogImpl;
	SearchResult* ExitImpl;
	SearchResult* NopImpl;
  
  const static unsigned NUM_INSTRUCTIONS = 1000;
	const static unsigned NUM_ITERATIONS = 10000;
	const static unsigned INITIAL_DEPTH = 5;
	const static unsigned SEARCH_DEPTH = 25;
	const static unsigned SEARCH_STEP = 3;

	bool IsReserved(const Register* Reg) const;
	const Register* GetLastAuxiliar() const;
  
public:  
  AsmProfileGen(TransformationRules& TR, InstrManager &IR,
		RegClassManager &RCM, OperandTableManager& OTM,
		OperatorTableManager& OtTM, PatternManager& PM):
		RuleManager(TR), InstructionManager(IR),
		RegisterClassManager(RCM), OperandTable(OTM),
		OperatorTable(OtTM), PatMan(PM) {
		PrologImpl = 0;
		IncRegImpl = 0;
		EpilogImpl = 0;
		ExitImpl = 0;
		NopImpl = 0;
  }

	~AsmProfileGen() {
		if (PrologImpl) {
			delete PrologImpl;
			PrologImpl = 0;
		}
		if (IncRegImpl) {
			delete IncRegImpl;			
			IncRegImpl = 0;
		}		
		if (EpilogImpl) {
			delete EpilogImpl;			
			EpilogImpl = 0;
		}		
		if (ExitImpl) {
			delete ExitImpl;
			ExitImpl = 0;
		}
		if (NopImpl) {
			delete NopImpl;
			NopImpl = 0;
		}
	}
  
  void SetWorkingDir (const char * wdir) { WorkingDir = wdir; }
  void SetCommentChar (char CommentChar) { this->CommentChar = CommentChar; }
  
  void EmitRandomizedInstruction(InstrIterator Ins, SemanticIterator SI, 
				 std::ostream &O);
	SearchResult *FindImplementation(const Tree* Exp, std::ostream &Log);
	std::list<const Register*>* GetAuxiliarList() const;
	void PrintPatternImpl(SearchResult *SR, std::ostream &O);
	void GenerateProlog(std::ostream &O, const std::string &RegName);
	void GenerateEpilog(std::ostream &O, const std::string &RegName,
											unsigned Threshold);
  void GenerateAssemblyTest(InstrIterator Ins, std::ostream &O);
  void GenerateFiles();
	void InferNop();
  void Generate();
};

}


#endif
