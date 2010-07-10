//===-- __arch__`'AsmPrinter.cpp - __arch__ LLVM assembly writer --------------------===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to GAS-format __arch__ assembly language.
// (GAS should also be retargeted by ArchC tools).
//
//===----------------------------------------------------------------------===//

`#define DEBUG_TYPE "'__arch__`-asm-printer"'

`#include "'__arch__`.h"'
`#include "'__arch__`Subtarget.h"'
`#include "'__arch__`InstrInfo.h"'
`#include "'__arch__`TargetMachine.h"'
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Target/TargetAsmInfo.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Support/Mangler.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <cctype>

using namespace llvm;

STATISTIC(EmittedInsts, "Number of machine instrs printed");

namespace {
  struct VISIBILITY_HIDDEN __arch__`'AsmPrinter : public AsmPrinter {

    const __arch__`'Subtarget *Subtarget;

    __arch__`'AsmPrinter(raw_ostream &O, __arch__`'TargetMachine &TM, 
                   const TargetAsmInfo *T): 
                   AsmPrinter(O, TM, T) {
      Subtarget = &TM.getSubtarget<`'__arch__`'Subtarget>();
    }

    virtual const char *getPassName() const {
      return "`'__arch__ Assembly Printer";
    }

    bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo, 
                         unsigned AsmVariant, const char *ExtraCode);
    void printOperand(const MachineInstr *MI, int opNum);
    void printLiteral(const MachineInstr *MI, int opNum);
    void printUnsignedImm(const MachineInstr *MI, int opNum);
    void printMemOperand(const MachineInstr *MI, int opNum, 
                         const char *Modifier = 0);
    void printFCCOperand(const MachineInstr *MI, int opNum, 
                         const char *Modifier = 0);
    void printModuleLevelGV(const GlobalVariable* GVar);
    void printHex32(unsigned int Value);

    const char *emitCurrentABIString(void);
    void emitFunctionStart(MachineFunction &MF);
    void emitFunctionEnd(MachineFunction &MF);
    void emitFrameDirective(MachineFunction &MF);

    bool printInstruction(const MachineInstr *MI);  // autogenerated.
    bool runOnMachineFunction(MachineFunction &F);
    bool doInitialization(Module &M);
    bool doFinalization(Module &M);
  };
} // end of anonymous namespace

`#include "'__arch__`GenAsmWriter.inc"'

/// create`'__arch__`'CodePrinterPass - Returns a pass that prints the __arch_in_caps__
/// assembly code for a MachineFunction to the given output stream,
/// using the given target machine description.  This should work
/// regardless of whether the function is in SSA form.
namespace llvm {

`FunctionPass *create'__arch__`'CodePrinterPass(raw_ostream &o,
                                              __arch__`'TargetMachine &tm) 
{
  return new __arch__`'AsmPrinter(o, tm, tm.getTargetAsmInfo());
}

}

//===----------------------------------------------------------------------===//
// Mask directives
//===----------------------------------------------------------------------===//



// Print a 32 bit hex number with all numbers.
void __arch__`'AsmPrinter::
printHex32(unsigned int Value) 
{
  O << "0x";
  for (int i = 7; i >= 0; i--) 
    O << utohexstr( (Value & (0xF << (i*4))) >> (i*4) );
}

//===----------------------------------------------------------------------===//
// Frame and Set directives
//===----------------------------------------------------------------------===//

/// Emit the directives used by GAS on the start of functions
void __arch__`'AsmPrinter::
emitFunctionStart(MachineFunction &MF)
{
  // Print out the label for the function.
  const Function *F = MF.getFunction();
  SwitchToSection(TAI->SectionForGlobal(F));

  // 2 bits aligned
  EmitAlignment(2, F);

  O << "\t.globl\t"  << CurrentFnName << '\n';
  O << "\t.ent\t"    << CurrentFnName << '\n';

  printVisibility(CurrentFnName, F->getVisibility());

  if ((TAI->hasDotTypeDotSizeDirective()) )
    O << "\t.type\t"   << CurrentFnName << ", @function\n";

  O << CurrentFnName << ":\n";

  O << '\n';
}

/// Emit the directives used by GAS on the end of functions
void __arch__`'AsmPrinter::
emitFunctionEnd(MachineFunction &MF) 
{
  
}

/// runOnMachineFunction - This uses the printMachineInstruction()
/// method to print assembly for each instruction.
bool __arch__`'AsmPrinter::
runOnMachineFunction(MachineFunction &MF) 
{
  SetupMachineFunction(MF);

  // Print out constants referenced by the function
  EmitConstantPool(MF.getConstantPool());

  // Print out jump tables referenced by the function
  EmitJumpTableInfo(MF.getJumpTableInfo(), MF);

  O << "\n\n";

  // Emit the function start directives
  emitFunctionStart(MF);

  // Print out code for the function.
  for (MachineFunction::const_iterator I = MF.begin(), E = MF.end();
       I != E; ++I) {

    // Print a label for the basic block.
    if (I != MF.begin()) {
      printBasicBlockLabel(I, true, true);
      O << '\n';
    }

    for (MachineBasicBlock::const_iterator II = I->begin(), E = I->end();
         II != E; ++II) {
      // Print the assembly for the instruction.
      printInstruction(II);
      ++EmittedInsts;
    }

    // Each Basic Block is separated by a newline
    O << '\n';
  }

  // Emit function end directives
  emitFunctionEnd(MF);

  // We didn't modify anything.
  return false;
}

// Print out an operand for an inline asm expression.
bool __arch__`'AsmPrinter::
PrintAsmOperand(const MachineInstr *MI, unsigned OpNo, 
                unsigned AsmVariant, const char *ExtraCode) 
{
  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0]) 
    return true; // Unknown modifier.

  printOperand(MI, OpNo);
  return false;
}

void __arch__`'AsmPrinter::
printLiteral(const MachineInstr *MI, int opNum) 
{

}

void __arch__`'AsmPrinter::
printOperand(const MachineInstr *MI, int opNum) 
{
  
}

void __arch__`'AsmPrinter::
printUnsignedImm(const MachineInstr *MI, int opNum) 
{
  const MachineOperand &MO = MI->getOperand(opNum);
  if (MO.getType() == MachineOperand::MO_Immediate)
    O << (unsigned short int)MO.getImm();
  else 
    printOperand(MI, opNum);
}

void __arch__`'AsmPrinter::
printMemOperand(const MachineInstr *MI, int opNum, const char *Modifier) 
{
  // when using stack locations for not load/store instructions
  // print the same way as all normal 3 operand instructions.
  if (Modifier && !strcmp(Modifier, "stackloc")) {
    printOperand(MI, opNum+1);
    O << ", ";
    printOperand(MI, opNum);
    return;
  }

  // Load/Store memory operands -- imm($reg) 
  // If PIC target the target is loaded as the 
  // pattern lw $25,%call16($28)
  printOperand(MI, opNum);
  O << "(";
  printOperand(MI, opNum+1);
  O << ")";
}

void __arch__`'AsmPrinter::
printFCCOperand(const MachineInstr *MI, int opNum, const char *Modifier) 
{
  
}

bool __arch__`'AsmPrinter::
doInitialization(Module &M) 
{
  Mang = new Mangler(M, "", TAI->getPrivateGlobalPrefix());

  return false; // success
}

bool __arch__`'AsmPrinter::
doFinalization(Module &M)
{
  O << '\n';

  return AsmPrinter::doFinalization(M);
}
