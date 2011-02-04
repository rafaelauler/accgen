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
#include <cstdlib>
#include <sstream>
#include <fstream>

using namespace backendgen;
using std::string;
using std::stringstream;
using std::endl;

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
    // Randomize register selection
    unsigned num = ((unsigned)rand()) % NumRegs;
    Operands.push_back(RC->getRegAt(num)->getName());    
  }
  // Emit ins
  (*Ins)->emitAssembly(Operands, SI, O);  
}

void AsmProfileGen::GenerateAssemblyTest(InstrIterator Ins, std::ostream &O) {
  SemanticIterator SI = (*Ins)->getBegin();    
  O << "main:" << endl;
  for (unsigned i = 0; i < NUM_INSTRUCTIONS; ++i) {
    EmitRandomizedInstruction(Ins, SI, O);
  }
}

// For each instruction, generate an assembly testbench file for it.
void AsmProfileGen::GenerateFiles() {  
  for (InstrIterator I = InstructionManager.getBegin(), 
	 E = InstructionManager.getEnd(), Prev = InstructionManager.getEnd();
       I != E; Prev = I++) {
    if ((*I)->getBegin() == (*I)->getEnd())
      continue; // This instructions does not have its behavior described by
	        // semantics in compiler_info.ac. Skipping it...
    string filename = WorkingDir;
    filename += "/";
    filename += (*I)->getLLVMName();
    filename += ".s";
    std::ofstream strm(filename.c_str(), std::ios::out | std::ios::trunc);
    GenerateAssemblyTest(I, strm);
    strm.close();
  }
}

void AsmProfileGen::Generate()
{
  GenerateFiles();
}
