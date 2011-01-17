//===-- __arch__`'TargetAsmInfo.cpp - __arch__ asm properties -----------*- C++ -*-===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the __arch__`'TargetAsmInfo properties.
//
//===----------------------------------------------------------------------===//

`#include "'__arch__`TargetAsmInfo.h"'

using namespace llvm;

__arch__`'ELFTargetAsmInfo::`'__arch__`'ELFTargetAsmInfo(const TargetMachine &TM):
  ELFTargetAsmInfo(TM) {
  Data16bitsDirective = "\t.half\t";
  Data32bitsDirective = "\t.word\t";
  Data64bitsDirective = 0;  
  ZeroDirective = "\t.skip\t";
  AlignmentIsInBytes = false;
  CommentString = "`'__comment_char__`'";
  ConstantPoolSection = "\t.section \".rodata\",#alloc\n";
  COMMDirectiveTakesAlignment = true;
  //CStringSection=".rodata.str";
  CStringSection=".data";

  BSSSection_  = getNamedSection("\t.bss",
                                 SectionFlags::Writeable | SectionFlags::BSS,
                                 /* Override */ true);
}

std::string __arch__`'ELFTargetAsmInfo::printSectionFlags(unsigned flags) const {
  if (flags & SectionFlags::Mergeable)
    return ELFTargetAsmInfo::printSectionFlags(flags);

  std::string Flags;
  if (!(flags & SectionFlags::Debug))
    Flags += ",#alloc";
  if (flags & SectionFlags::Code)
    Flags += ",#execinstr";
  if (flags & SectionFlags::Writeable)
    Flags += ",#write";
  if (flags & SectionFlags::TLS)
    Flags += ",#tls";

  return Flags;
}
