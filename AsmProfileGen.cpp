//===- AsmProfileGen.cpp ------------------------------------*- C++ -*-----===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// In this class, we define member functions responsible for controlling the
// process of assembly profile files generation. Each of these files
// extensively tests a single machine instruction, by changing its operands,
// replicating it several times and embedding it inside a loop.
//
//===----------------------------------------------------------------------===//

#include "AsmProfileGen.h"
#include "Support.h"
#include <cstdlib> 
#include <sstream>
#include <fstream>

using namespace backendgen;
using std::string;
using std::stringstream;
using std::endl;
using std::list;

inline bool AsmProfileGen::IsReserved(const Register *Reg) const {
	for (std::set<Register*>::const_iterator I = 
				 RegisterClassManager.getReservedBegin(), 
				 E = RegisterClassManager.getReservedEnd(); I != E; ++I) {
		if (Reg == &**I)
			return true;
	}
	return false;
}

void AsmProfileGen::EmitRandomizedInstruction(InstrIterator Ins,
																							SemanticIterator SI,
																							std::ostream &O) {
  CnstOperandsList *AllOps = (*Ins)->getOperandsBySemantic();
  std::list<std::string> Operands;
  // Build random operands list
  for (CnstOperandsList::const_iterator I = AllOps->begin(), 
	   E = AllOps->end(); I != E; ++I) {
    const RegisterOperand* RO = dynamic_cast<const RegisterOperand*>(*I);
    // If not a register, treat it as immediate
    if (RO == NULL) { 
      // Type 0 is a reserved value for "dummy operand", meaning we don't
      // know what kind this operand is (it is never mentioned in the 
      // semantic tree).
      if ((*I)->getType() == 0) {
				//Operands.push_back("");
      } else {
				// Randomize immediate
				unsigned num = ((unsigned)rand()) % 10;
				stringstream SS;
				SS << num;
				Operands.push_back(SS.str());
      }
      continue;
    }
    const RegisterClass* RC = RO->getRegisterClass();
    unsigned NumRegs = RC->getNumRegs();
		assert (NumRegs > 0 && "Using register class with no registers");
		const Register* reg;
		unsigned trials = 0;
		do {
			// Randomize register selection
			unsigned num = ((unsigned)rand()) % NumRegs;
			reg = RC->getRegAt(num);
			assert (reg && "getRegAt() didn't return a valid register");
			++trials;
			// Check if it is a reserved register
		} while (trials < 100 && IsReserved(reg));
		if (trials == 100) {
			std::cerr << "Error in assembler profile code generator:"
				" Could not select a random register. Check if all registers are reserved"
				" in register class \"" << RC->getName() << "\".\n";
			abort();
		}
    Operands.push_back(reg->getAssemblyName());    
  }
  // Emit ins
  (*Ins)->emitAssembly(Operands, SI, O);  
}

SearchResult* AsmProfileGen::FindImplementation(const Tree* Exp, std::ostream &Log) {
  Search S(RuleManager, InstructionManager);
  unsigned SearchDepth = INITIAL_DEPTH;
  SearchResult *R = NULL;
  // Increasing search depth loop - first try with low depth to speed up
  // easy matches
  while (R == NULL || R->Instructions->size() == 0) {
    if (SearchDepth >= SEARCH_DEPTH)
      break;
		Log << "  Trying search with depth " << SearchDepth << "\n";
    S.setMaxDepth(SearchDepth);
    SearchDepth = SearchDepth + SEARCH_STEP;
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

inline list<const Register*>* AsmProfileGen::GetAuxiliarList() const {
	list<const Register*>* Result = new list<const Register*>();
	for (std::set<Register*>::const_iterator I = 
				 RegisterClassManager.getAuxiliarBegin(), 
				 E = RegisterClassManager.getAuxiliarEnd(); I != E; ++I) {
		Result->push_back(*I);
	}
	return Result;
}

// Receives a list of instructions, corresponding to a pattern implementation
// in SearchResult record, and prints their assembly syntax to ostream.
void AsmProfileGen::PrintPatternImpl(SearchResult *SR, std::ostream &O) {
	list<const Register*>* auxiliarRegs = GetAuxiliarList();					 
	RegAlloc *RA = NULL;
  assert(auxiliarRegs->size() > 0 && "Must provide auxiliar regs list");  
  RA = RegAlloc::Build(auxiliarRegs, SR->OperandsDefs->begin(),
											 SR->OperandsDefs->end());  
  
  OperandsDefsType::const_iterator OI = SR->OperandsDefs->begin();
  for (InstrList::const_iterator I = SR->Instructions->begin(),
	 E = SR->Instructions->end(); I != E; ++I) {
    NameListType* OpNames = *(OI++);
		// Filter some operand names (transform const<99> -> 99, performs
		// simple register allocation when virtual registers are used)
		// Virtual registers are identified by "AReg999"
		for (NameListType::iterator I2 = OpNames->begin(), E2 = OpNames->end();
				 I2 != E2; ++I2) {
			if (!I2->substr(0,4).compare("AReg")) {
				*I2 = RA->getPhysRegisterRef(*I2)->getAssemblyName();
				continue;
			}
			int val;
			if (ExtractConstValue(*I2, &val)) {
				stringstream SS;
				SS << val;
				*I2 = SS.str();
				continue;
			}
		}
		// Print instruction assembly syntax to ostream
		I->first->emitAssembly(*OpNames, I->second, O);
		RA->NextInstruction();
		if (I->first->HasDelaySlot()) {
			assert (SR != NopImpl && "Nop implementation should not use delay slots");
		  PrintPatternImpl(NopImpl, O);
		}
	}
	
	delete auxiliarRegs;
}

//=====
// The general structure of the profile assembly program is
// PROLOG - initialize a chosen register with 0
// BODY - contains NUM_INSTRUCTIONS copies of the tested instruction
// EPILOG - increments the chosen register and loops if it is lower than
//          a certain threshold.
// EXIT - call clib "exit" function to terminate program execution
//====

// This functions generates the prolog (initializes the chosen register
// RegName with 0
void AsmProfileGen::GenerateProlog(std::ostream &O, const string &RegName) {
	if (PrologImpl) {
		PrintPatternImpl(PrologImpl, O);
		O << "loop_begin:\n";
		return;
	}
	const Tree* Exp = PatternManager::genZeroRegPat(OperatorTable,
																									OperandTable,
																									RegisterClassManager,
																									"regs", RegName);
	std::cout << "Searching for implementation of:\n ";
	Exp->print(std::cout);
	std::cout << "\n";
	SearchResult *SR = FindImplementation(Exp, std::cerr);
	if (!SR) {
		std::cerr << "Error in assembler profile code generator:";
		std::cerr << " Could not find a machine implementation for prolog.\n";
		abort();
	}
	SR->FilterAssignedNames();
	PrologImpl = SR;
	delete Exp;
	PrintPatternImpl(SR, O);
	O << "loop_begin:\n";
}

// This member function generates the epilog (increments the chosen
// register RegName and loops if lower than the threshold). Also calls
// exit function if loop ended.
void AsmProfileGen::GenerateEpilog(std::ostream &O, const string &RegName,
																	 unsigned Threshold) {
	// If we already infered the necessary patterns, just print them.
	if (EpilogImpl) {
		PrintPatternImpl(IncRegImpl, O);
		PrintPatternImpl(EpilogImpl, O);
		PrintPatternImpl(ExitImpl, O);
		return;
	}
	// Try to guess how to increment register in this machine..
	const Tree* Exp = PatternManager::genIncRegPat(OperatorTable,
																								 OperandTable,
																								 RegisterClassManager,
																								 "regs",
																								 RegName);
	std::cout << "Searching for implementation of:\n ";
	Exp->print(std::cout);
	std::cout << "\n";
	SearchResult *SR = FindImplementation(Exp, std::cerr);
	if (!SR) {
		std::cerr << "Error in assembler profile code generator:";
		std::cerr << " Could not find a machine implementation for register increment.\n";
		abort();
	}
	SR->FilterAssignedNames();
	PrintPatternImpl(SR, O);
	IncRegImpl = SR;
	delete Exp;
	// Try to guess how to compare and branch on lower in this
	// machine...
	Exp = PatternManager::genBranchLtImmPat(OperatorTable,
																					OperandTable,
																					RegisterClassManager,
																					"regs",
																					RegName,
																					"loop_begin",
																					Threshold);
	std::cout << "Searching for implementation of:\n ";
	Exp->print(std::cout);
	std::cout << "\n";
	SR = FindImplementation(Exp, std::cerr);
	if (!SR) {
		std::cerr << "Error in assembler profile code generator:";
		std::cerr << " Could not find a machine implementation for epilog.\n";
		abort();
	}
	SR->FilterAssignedNames();
	delete Exp;
	PrintPatternImpl(SR, O);
	EpilogImpl = SR;
	// Try to guess how to make a call to exit function in this machine...
	Exp = PatternManager::genCallPat(OperatorTable, OperandTable,	"exit");
	std::cout << "Searching for implementation of:\n ";
	Exp->print(std::cout);
	std::cout << "\n";
	SR = FindImplementation(Exp, std::cerr);
	if (!SR) {
		std::cerr << "Error in assembler profile code generator:";
		std::cerr << " Could not find a machine implementation for function calls.\n";
		abort();
	}
	SR->FilterAssignedNames();
	PrintPatternImpl(SR, O);
	ExitImpl = SR;
	delete Exp;
}

void AsmProfileGen::InferNop() {
	// Try to guess how to increment register in this machine..
	const Tree* Exp = PatternManager::genNopPat(OperatorTable,
																							OperandTable);																				 
	std::cout << "Searching for implementation of:\n ";
	Exp->print(std::cout);
	std::cout << "\n";
	SearchResult *SR = FindImplementation(Exp, std::cerr);
	if (!SR) {
		std::cerr << "Error in assembler profile code generator:";
		std::cerr << " Could not find a machine implementation for NOP.\n";
		abort();
	}
	SR->FilterAssignedNames();
	NopImpl = SR;
	delete Exp;
}


inline const Register* AsmProfileGen::GetLastAuxiliar() const {
	const Register* Result = 0;
	for (std::set<Register*>::const_iterator 
				 I = RegisterClassManager.getAuxiliarBegin(),
				 E = RegisterClassManager.getAuxiliarEnd(); I != E; ++I) {
		Result = *I;
	}
	return Result;
}

// Generates the aforementioned profile assembly program in its whole 
// structure. This profile will test the behavior of instruction Ins
// and print the program to ostream O.
void AsmProfileGen::GenerateAssemblyTest(InstrIterator Ins, std::ostream &O) {
  SemanticIterator SI = (*Ins)->getBegin();    
	O << ".globl main" << endl;
  O << "main:" << endl;
	const Register* TempReg = GetLastAuxiliar();
	assert(TempReg != 0 && "Must have auxiliar registers."); 
	GenerateProlog(O, TempReg->getAssemblyName());
  for (unsigned i = 0; i < NUM_INSTRUCTIONS; ++i) {
    EmitRandomizedInstruction(Ins, SI, O);
  }
	GenerateEpilog(O, TempReg->getAssemblyName(), NUM_ITERATIONS);
}

// For each instruction, generate an assembly testbench file for it.
void AsmProfileGen::GenerateFiles() {  
	// First try to guess how to print NOPs in this architecture, 
	// which will later be needed to fill delay slots.
	InferNop();

  for (InstrIterator I = InstructionManager.getBegin(), 
	 E = InstructionManager.getEnd(), Prev = InstructionManager.getEnd();
       I != E; Prev = I++) {
    if ((*I)->getBegin() == (*I)->getEnd())
      continue; // This instructions does not have its behavior described by
		            // semantics in compiler_info.ac. Skipping it...
		if ((*I)->isCall() || (*I)->isJump() || (*I)->isReturn())
			continue; // Not interested in testing instruction which can change
		            // program flow.
    string filename = WorkingDir;
    filename += "/";
    filename += (*I)->getLLVMName();
    filename += ".s";
    std::ofstream strm(filename.c_str(), std::ios::out | std::ios::trunc);
    GenerateAssemblyTest(I, strm);
    strm.close();
  }
}

// Class entry point
void AsmProfileGen::Generate()
{
  GenerateFiles();
}
