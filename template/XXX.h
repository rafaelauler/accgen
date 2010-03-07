//===-- __arch__`'.h - Top-level interface for __arch__ representation --*- C++ -*-===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// __arch__ back-end.
//
//===----------------------------------------------------------------------===//

`#ifndef TARGET_'__arch_in_caps__`_H'
`#define TARGET_'__arch_in_caps__`_H'

#include <cassert>

namespace llvm {
  class FunctionPass;
  class TargetMachine;
  class __arch__`TargetMachine;'
  class raw_ostream;

  FunctionPass *create`'__arch__`'ISelDag(`'__arch__`'TargetMachine &TM);
  FunctionPass *create`'__arch__`'CodePrinterPass(raw_ostream &OS, TargetMachine &TM);
} // end namespace llvm;

// Defines symbolic names for __arch__ registers.  This defines a mapping from
// register name to register number.
//
`#include "'__arch__`GenRegisterNames.inc"'

// Defines symbolic names for the __arch__ instructions.
//
`#include "'__arch__`GenInstrNames.inc"'


#endif
