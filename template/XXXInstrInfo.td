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

//===----------------------------------------------------------------------===//
// Type profiles
//===----------------------------------------------------------------------===//
def SDT_Ret      : SDTypeProfile<0, 1, [SDTCisInt<0>]>;
def SDT_Call     : SDTypeProfile<0, 1, [SDTCisVT<0, iPTR>]>;
def SDT_CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>]>;
def SDT_CallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;

//===----------------------------------------------------------------------===//
// Target specific nodes
//===----------------------------------------------------------------------===//
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_CallSeqStart,
                           [SDNPHasChain, SDNPOutFlag]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_CallSeqEnd,
                           [SDNPHasChain, SDNPOptInFlag, SDNPOutFlag]>;
def Call : SDNode<"`'__arch__`'ISD::CALL",SDT_Call, [SDNPHasChain,
                         SDNPOutFlag]>;

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


