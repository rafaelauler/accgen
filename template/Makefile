##===- lib/Target/Test/Makefile ----------------------------*- Makefile -*-===##
#
#                     The ArchC LLVM Backend Generator
#
##===----------------------------------------------------------------------===##
LEVEL = ../../..
LIBRARYNAME = LLVM`'__arch__`'
TARGET = `'__arch__`'

# Make sure that tblgen is run, first thing.
BUILT_SOURCES = MipsGenRegisterInfo.h.inc MipsGenRegisterNames.inc \
                MipsGenRegisterInfo.inc MipsGenInstrNames.inc \
                MipsGenInstrInfo.inc MipsGenAsmWriter.inc \
                MipsGenDAGISel.inc MipsGenCallingConv.inc \
                MipsGenSubtarget.inc

include $(LEVEL)/Makefile.common

