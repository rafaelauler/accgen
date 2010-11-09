//===-- __arch__`'TargetMachine.cpp - Define TargetMachine for __arch__ ---===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

`#include "'__arch__`TargetAsmInfo.h"'
`#include "'__arch__`TargetMachine.h"'
`#include "'__arch__`.h"'
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Target/TargetMachineRegistry.h"
using namespace llvm;

/// __arch__`'TargetMachineModule - Note that this is used on hosts that
/// cannot link in a library unless there are references into the
/// library.  In particular, it seems that it is not possible to get
/// things to work on Win32 without this.  Though it is unused, do not
/// remove it.
extern "C" int __arch__`'TargetMachineModule;
int __arch__`'TargetMachineModule = 0;

// Register the target.
static RegisterTarget<`'__arch__`'TargetMachine> X("`'__arch__`'", "`'__arch_in_caps__`'");

`const TargetAsmInfo *'__arch__`TargetMachine::createTargetAsmInfo() const {'
  return new __arch__`'ELFTargetAsmInfo(*this);
}

/// __arch__`'TargetMachine ctor
///
__arch__`'TargetMachine::`'__arch__`'TargetMachine(const Module &M, const std::string &FS)
  : `DataLayout("'__data_layout_string__`"),'
    Subtarget(M, FS), TLInfo(*this), InstrInfo(Subtarget),
    FrameInfo(__stack_growth__, __stack_alignment__, 0) {
}

unsigned __arch__`'TargetMachine::getModuleMatchQuality(const Module &M) {
  std::string TT = M.getTargetTriple();
  if (TT.size() >= 6 && std::string(TT.begin(), TT.begin()+6) == "`'__arch__`'-")
    return 20;
  
  // If the target triple is something non-`'__arch__`', we don't match.
  if (!TT.empty()) return 0;

  if (M.getEndianness()  == Module::BigEndian &&
      M.getPointerSize() == Module::Pointer32)
`#ifdef __'__arch__`__'
    return 20;   // BE/32 ==> Prefer __arch__ on __arch__
#else
    return 5;    // BE/32 ==> Prefer ppc elsewhere
#endif
  else if (M.getEndianness() != Module::AnyEndianness ||
           M.getPointerSize() != Module::AnyPointerSize)
    return 0;                                    // Match for some other target

`#if defined(__'__arch__`__)'
  return 10;
#else
  return 0;
#endif
}

bool __arch__`'TargetMachine::addInstSelector(PassManagerBase &PM, bool Fast) {
  PM.add(create`'__arch__`'ISelDag(*this));
  return false;
}

/// addPreEmitPass - This pass may be implemented by targets that want to run
/// passes immediately before machine code is emitted.  This should return
/// true if -print-machineinstrs should print out the code after the passes.
bool __arch__`'TargetMachine::addPreEmitPass(PassManagerBase &PM, bool Fast) {
  return true;
}

bool __arch__`'TargetMachine::addAssemblyEmitter(PassManagerBase &PM, bool Fast, 
                                            raw_ostream &Out) {
  // Output assembly language.
  PM.add(create`'__arch__`'CodePrinterPass(Out, *this));
  return false;
}

