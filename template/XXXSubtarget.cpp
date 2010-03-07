//===- __arch__`'Subtarget.cpp - __arch_in_caps__ Subtarget Information -------------------===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file implements the __arch_in_caps__ specific subclass of TargetSubtarget.
//
//===----------------------------------------------------------------------===//

`#include "'__arch__`Subtarget.h"'
`#include "'__arch__`GenSubtarget.inc"'
using namespace llvm;

__arch__`'Subtarget::`'__arch__`'Subtarget(const Module &M, const std::string &FS) {
  // Determine default and user specified characteristics
  std::string CPU = "generic";

  IsV9 = false;
  
  // Parse features string.
  ParseSubtargetFeatures(FS, CPU);
}
