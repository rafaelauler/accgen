//===- __arch__`InstrInfo.td' - __arch__ Instruction defs --------------------*- C++ -*-===//
//
//                     The ArchC LLVM Backend Generator
//
//   This file was automatically generated from an ArchC description.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Instruction format superclass
//===----------------------------------------------------------------------===//

include `"'__arch__`InstrFormats.td"'


// These are target-independent nodes, but have target-specific formats.
//def callseq_start : SDNode<"ISD::CALLSEQ_START", SDTLoad,
//                           []>;
//def callseq_end   : SDNode<"ISD::CALLSEQ_END",   SDTStore,
//                           []>;

//===----------------------------------------------------------------------===//
// __arch__ instruction definitions
//===----------------------------------------------------------------------===//

__instructions_definitions__

//===----------------------------------------------------------------------===//
// __arch__ pseudo instruction definitions
//===----------------------------------------------------------------------===//
class Pseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
   : InstSP16_0<outs, ins, asmstr, pattern>;


def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i32imm:$amt),
                               "!ADJCALLSTACKDOWN $amt",
                               []>;
def ADJCALLSTACKUP : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                            "!ADJCALLSTACKUP $amt1",
                            []>;


