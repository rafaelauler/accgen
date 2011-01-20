//==-- TemplateManager.cpp
// Uses template files (living in ./template) to produce code to the
// LLVM backend. Manages data necessary to transform template into 
// specific code to the target architecture.

#include "Support.h"
#include "TemplateManager.h"
#include "InsnFormat.h"
#include "SaveAgent.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cassert>
#include <ctime>

#define INITIAL_DEPTH 5
#define SEARCH_DEPTH 25
#define SEARCH_STEP 3

using namespace backendgen;
using namespace backendgen::expression;
using std::map;
using std::list;
using std::set;
using std::string;
using std::stringstream;
using std::endl;
using std::pair;
using std::make_pair;

// Creates the M4 file that defines macros expansions to be
// performed in template files. These expansions are defines as
// target specific values, so there is no point calling this function
// if all the data needed is not available yet.
void TemplateManager::CreateM4File()
{
  //  std::locale loc;
  string FileName = WorkingDir;
  string ArchNameCaps;
  string *Funcs, *Switch, *Headers;
  const Register* LR = RegisterClassManager.getReturnRegister();
  const Register* FP = RegisterClassManager.getFramePointer();
  const Register* PC = RegisterClassManager.getProgramCounter();
  const Register* SP = RegisterClassManager.getStackPointer();
  int PCOffset = RegisterClassManager.getPCOffset();
  std::transform(ArchName.begin(), ArchName.end(), 
		 std::back_inserter(ArchNameCaps), 
		 std::ptr_fun(toupper));
  FileName += "/macro.m4";
  std::ofstream O(FileName.c_str(), std::ios::out);

  // Creates logtable macro
  O << "m4_define(`m4log', `m4_ifelse($1,1,1,m4_ifelse($1,2,1,m4_ifelse($1," <<
    "4,2,m4_ifelse($1,8,3,m4_ifelse($1,16,4,m4_ifelse($1,24,5,m4_ifelse($1," <<
    "32,5,m4_ifelse($1,48,6,m4_ifelse($1,64,6)))))))))')m4_dnl\n";

  // Defines an expansion for each target specific data
  O << "m4_define(`__arch__',`" << ArchName << "')m4_dnl\n"; 
  O << "m4_define(`__arch_in_caps__',`" << ArchNameCaps << "')m4_dnl\n"; 
  O << "m4_define(`__nregs__',`" << NumRegs << "')m4_dnl\n"; 
  O << "m4_define(`__wordsize__',`" << WordSize << "')m4_dnl\n"; 
  O << "m4_define(`__comment_char__',`" << CommentChar << "')m4_dnl\n"; 
  O << "m4_define(`__type_char_specifier__',`" << TypeCharSpecifier 
    << "')m4_dnl\n"; 
  O << "m4_define(`__ra_register__',`" << ArchName << "::"
    << LR->getName() << "')m4_dnl\n";
  O << "m4_define(`__pc_asm__',`" << PC->getName() << "')m4_dnl\n";
  O << "m4_define(`__frame_register__',`" << ArchName <<  "::"
    << FP->getName() << "')m4_dnl\n";
  O << "m4_define(`__stackpointer_register__',`" << ArchName <<  "::"
    << SP->getName() << "')m4_dnl\n";
  if (!RegisterClassManager.getGrowsUp()) {
    O << "m4_define(`__stack_growth__',`TargetFrameInfo::StackGrowsDown"
         "')m4_dnl\n"; 
  } else {
    O << "m4_define(`__stack_growth__',`TargetFrameInfo::StackGrowsUp"
         "')m4_dnl\n";
  }
  O << "m4_define(`__stack_alignment__',`" 
    << RegisterClassManager.getAlignment() << "')m4_dnl\n"; 
  O << "m4_define(`__registers_definitions__',`";
  O << generateRegistersDefinitions() << "')m4_dnl\n";
  O << "m4_define(`__register_classes__',`";
  O << generateRegisterClassesDefinitions() << "')m4_dnl\n";
  O << "m4_define(`__callee_saved_reg_list__',`";
  O << generateCalleeSaveList() << "')m4_dnl\n";
  O << "m4_define(`__callee_saved_reg_classes_list__',`";
  O << generateCalleeSaveRegClasses() << "')m4_dnl\n";
  O << "m4_define(`__reserved_list__',`";
  O << generateReservedRegsList() << "')m4_dnl\n";
  O << "m4_define(`__instructions_definitions__',`";
  O << generateInstructionsDefs() << "')m4_dnl\n";
  O << "m4_define(`__size_table__',`";
  O << generateInsnSizeTable() << "')m4_dnl\n";
  O << "m4_define(`__pc_offset__',`";
  O << PCOffset << "')m4_dnl\n";  
  O << "m4_define(`__calling_conventions__',`";
  O << generateCallingConventions() << "')m4_dnl\n";
  O << "m4_define(`__data_layout_string__',`";
  O << buildDataLayoutString() << "')m4_dnl\n";
  O << "m4_define(`__return_lowering__',`";
  O << generateReturnLowering() << "')m4_dnl\n";  
  O << "m4_define(`__set_up_register_classes__',`";
  O << generateRegisterClassesSetup() << "')m4_dnl\n";
  generateSimplePatterns(std::cout, &Funcs, &Switch, &Headers);
  O << "m4_define(`__simple_patterns__',`";  
  O << *Funcs << "')m4_dnl\n";
  O << "m4_define(`__patterns_switch__',`";  
  O << *Switch << "')m4_dnl\n";
  O << "m4_define(`__patterns_headers__',`";  
  O << *Headers << "')m4_dnl\n";
  
  O << "m4_define(`__copyregpats__',`";
  O << generateCopyRegPatterns(std::cout) << "')m4_dnl\n";
  O << "m4_define(`__is_move__',`";
  O << generateIsMove() << "')m4_dnl\n";
  O << "m4_define(`__eliminate_call_frame_pseudo_positive__',`";
  O << generateEliminateCallFramePseudo(std::cout, true) << "')m4_dnl\n";  
  O << "m4_define(`__eliminate_call_frame_pseudo_negative__',`";
  O << generateEliminateCallFramePseudo(std::cout, false) << "')m4_dnl\n";  
  O << "m4_define(`__emit_prologue__',`";
  O << generateEmitPrologue(std::cout) << "')m4_dnl\n";
  O << "m4_define(`__emit_epilogue__',`";
  O << generateEmitEpilogue(std::cout) << "')m4_dnl\n";
  O << "m4_define(`__insert_branch__',`";
  O << generateInsertBranch() << "')m4_dnl\n";
  O << "m4_define(`__emit_select_cc__',`";
  O << generateEmitSelectCC() << "')m4_dnl\n";
  O << "m4_define(`__select_cc_patts__',`";
  O << generateSelectCCTGenPatts() << "')m4_dnl\n";  
  O << "m4_define(`__store_reg_to_stack_slot__',`";      
  O << generateStoreRegToStackSlot() << "')m4_dnl\n";
  O << "m4_define(`__load_reg_from_stack_slot__',`";
  O << generateLoadRegFromStackSlot() << "')m4_dnl\n";      
  O << "m4_define(`__is_load_from_stack_slot__',`";
  O << generateIsLoadFromStackSlot() << "')m4_dnl\n";  
  O << "m4_define(`__is_store_to_stack_slot__',`";
  O << generateIsStoreToStackSlot() << "')m4_dnl\n";    
  O << "m4_define(`__masktgtimm__',`";
  O << generateTgtImmMask() << "')m4_dnl\n";    
  O << "m4_define(`__change_fi_to_32_bit__',`";
  O << generateFISwitchTo32Bit() << "')m4_dnl\n";    
  O << "m4_define(`__change_fi_to_16_bit__',`";
  O << generateFISwitchTo16Bit() << "')m4_dnl\n";    
  
  // Print literals are only available AFTER all pattern
  // generation inference!
  O << "m4_define(`__print_literal__',`";
  O << generatePrintLiteral() << "')m4_dnl\n";
  
  
  delete Funcs;
  delete Switch;
  delete Headers;

}

// Inline helper procedure to write a register definition into tablegen
// register file (XXXRegisterInfo.td)
inline void DefineRegister(stringstream &SS, Register *R,
			   unsigned Counter) {
  SS << "  def " << R->getName() << " : AReg<" << Counter;
  SS << ", \"" << R->getName() << "\", [], [";
  // Has subregisters
  if (R->getSubsBegin() != R->getSubsEnd()) {
    for (list<Register*>::const_iterator I = R->getSubsBegin(),
	   E = R->getSubsEnd(); I != E; ++I) {
      if (I != R->getSubsBegin())
	SS << ",";
      SS << (*I)->getName();
    }   
  } 
  SS << "]>;\n";
}

// Generates the XXXRegisterInfo.td registers definitions
string TemplateManager::generateRegistersDefinitions() {  
  stringstream SS;
  unsigned Counter = 0;
  set<Register*> DefinedRegisters;

  for (set<Register*>::const_iterator 
	 I = RegisterClassManager.getRegsBegin(), 
	 E = RegisterClassManager.getRegsEnd(); I != E; ++I) {
    // For each subregister , define it first if it is not defined already
    for (list<Register*>::const_iterator
	   I2 = (*I)->getSubsBegin(), E2 = (*I)->getSubsEnd();
	   I2 != E2; ++I2) {
      if (DefinedRegisters.find(*I2) == DefinedRegisters.end()) {
	DefineRegister(SS, *I2, Counter);
	++Counter;
	DefinedRegisters.insert(*I2);
      }
    }
    DefineRegister(SS, *I, Counter);
    ++Counter;
    DefinedRegisters.insert(*I);
  }

  return SS.str();
}

// This procedure build the LLVM data layout string used to initialize
// a DataLayout class living inside TargetMachine.
string TemplateManager::buildDataLayoutString() {
  stringstream SS;

  // Endianess
  if (IsBigEndian)
    SS << "E";
  else
    SS << "e";
  // Now for pointer information
  // XXX: Hardcoded prefered alignment as the same as pointer size.
  SS << "-p:" << WordSize << ":" << WordSize
     << "-f80" << ":" << WordSize << ":" << WordSize;

  return SS.str();
}

// Generates the list used in XXXRegisterInfo.cpp
string TemplateManager::generateCalleeSaveList() {
  stringstream SS;

  SS << "static const unsigned CalleeSavedRegs[] = {";

  for (set<Register*>::const_iterator 
	 I = RegisterClassManager.getCalleeSBegin(),
	 E = RegisterClassManager.getCalleeSEnd(); I != E; ++I) {
    SS << ArchName << "::" << (*I)->getName() << ", ";
  }
  SS << "0};";
  return SS.str();
}

// Helper function for generateCalleeSaveRegClasses()
inline string TemplateManager::getRegisterClass(Register* Reg) {
  RegisterClass* RC = RegisterClassManager.getRegRegClass(Reg);
  assert (RC != NULL && "Register has no associated Register Class");
  return RC->getName();
}

// Generates list used in XXXRegisterInfo.cpp
string TemplateManager::generateCalleeSaveRegClasses() {
  stringstream SS;

  SS << "static const TargetRegisterClass * const CalleeSavedRC[] = {";

  for (set<Register*>::const_iterator 
	 I = RegisterClassManager.getCalleeSBegin(),
	 E = RegisterClassManager.getCalleeSEnd(); I != E; ++I) {
    SS << "&" << ArchName << "::" << getRegisterClass(*I) << "RegClass" << ",";
  }
  SS << "0};";
  return SS.str();
}

// Generates a reserved regs list code used in XXXRegisterInfo.cpp
string TemplateManager::generateReservedRegsList() {
  stringstream SS;
  const Register* PC = RegisterClassManager.getProgramCounter();
  const Register* LR = RegisterClassManager.getReturnRegister();
  const Register* FP = RegisterClassManager.getFramePointer();
  bool isPCReserved = false;
  bool isLRReserved = false;
  bool isFPReserved = false;

  for (set<Register*>::const_iterator
	 I = RegisterClassManager.getReservedBegin(),
	 E = RegisterClassManager.getReservedEnd(); I != E; ++I) {
    SS << "  Reserved.set(" << ArchName << "::" << (*I)->getName() << ");"
       << endl;
    if (PC->getName() == (*I)->getName()) {
      isPCReserved = true;
    } else if (LR->getName() == (*I)->getName()) {
      isLRReserved = true;
    } else if (FP->getName() == (*I)->getName()) {
      isFPReserved = true;
    }
  }
  // Guarantee that these registers are always reserved
  if (!isPCReserved) {
    SS << "  Reserved.set(" << ArchName << "::" << PC->getName() << ");" 
       << endl;
  }
  if (!isLRReserved) {
    SS << "  Reserved.set(" << ArchName << "::" << LR->getName() << ");" 
       << endl;
  }
  if (!isFPReserved) {
    SS << "  Reserved.set(" << ArchName << "::" << FP->getName() << ");" 
       << endl;
  }
  return SS.str();
}

// Generates the register classes set up in ISelLowering.cpp
string TemplateManager::generateRegisterClassesSetup() {
  stringstream SS;

  for (set<RegisterClass*>::const_iterator
	 I = RegisterClassManager.getBegin(),
	 E = RegisterClassManager.getEnd(); I != E; ++I) {
    SS << "  addRegisterClass(MVT::" << InferValueType(*I) << ", "
       << ArchName << "::" << (*I)->getName() << "RegisterClass);\n";
  }

  return SS.str();
}

namespace {
inline bool isReserved(RegClassManager& RegisterClassManager, Register* reg) {
  for (set<Register*>::const_iterator
	 I = RegisterClassManager.getReservedBegin(),
	 E = RegisterClassManager.getReservedEnd(); I != E; ++I) {   
    if (reg->getName() == (*I)->getName()) {
      return true;
    } 
  }
  return false;
}
}

// Generates the XXXRegisterInfo.td register classes
string TemplateManager::generateRegisterClassesDefinitions() {
  stringstream SS;

  for (set<RegisterClass*>::const_iterator
	 I = RegisterClassManager.getBegin(),
	 E = RegisterClassManager.getEnd(); I != E; ++I) {
    SS << "def " << (*I)->getName() << ": RegisterClass<\"" << ArchName;
    // XXX: Support different alignments! (Hardcoded as 32)
    SS << "\", [" << InferValueType(*I) << "], 32, [";
    // Iterates through all registers in this class,
    // leaves reserved registers at the end of the list
    bool hasReserved = false;
    bool hasNonReserved = false;
    for (set<Register*>::const_iterator R = (*I)->getBegin(),
	   E2 = (*I)->getEnd(); R != E2; ++R) {
      if (isReserved(RegisterClassManager, *R)) {
	hasReserved = true;
	continue;
      }      
      if (hasNonReserved)
	SS << ",";
      SS << (*R)->getName();     
      hasNonReserved = true;
    }
    // print separator
    if (hasNonReserved && hasReserved)
      SS << ",";
    // Now print reserved registers
    unsigned numReserved = 0;
    for (set<Register*>::const_iterator R = (*I)->getBegin(),
	   E2 = (*I)->getEnd(); R != E2; ++R) {
      if (!isReserved(RegisterClassManager, *R))	
	continue;      
      if (numReserved > 0)
	SS << ",";
      SS << (*R)->getName();     
      ++numReserved;
    }
    SS << "]>";    
    if (!hasReserved) {
      SS << ";" << endl << endl;
      continue;
    } 
    // hasReserved = true, so now print method to avoid allocation of reserved
    // regs    
    SS << "{" << endl;
    SS << "  let MethodProtos = [{" << endl;
    SS << "    iterator allocation_order_end(const MachineFunction &MF) const;"
       << endl;
    SS << "  }];" << endl;
    SS << "  let MethodBodies = [{" << endl;
    SS << "    " << (*I)->getName() << "Class::iterator" << endl;
    SS << "    " << (*I)->getName() << "Class::allocation_order_end"
      "(const MachineFunction &MF) const {" << endl;      
    SS << "      return end()-" << numReserved <<";" << endl;
    SS << "    }" << endl;
    SS << "  }];" << endl;    
    SS << "}" << endl << endl;    
    // TODO: Support for infinite copy cost class of registers (registers
    // that shouldn't be copied    
  }

  return SS.str();
}

// Generate tablegen instructions definitions for XXXInstrInfo.td
string TemplateManager::generateInstructionsDefs() {
  stringstream SS;

  unsigned CurInsVersion = 0;
  for (InstrIterator I = InstructionManager.getBegin(), 
	 E = InstructionManager.getEnd(), Prev = InstructionManager.getEnd();
       I != E; Prev = I++) {
    if (Prev != E && (*Prev)->getName() != (*I)->getName())
      CurInsVersion = 0;
    // Define the instruction's LLVM name, a string tag used whenever we
    // reference this instruction in LLVM generated code.
    {
      stringstream SStmp;
      SStmp << (*I)->getName() << "_" << ++CurInsVersion;
      string LLVMName = SStmp.str();
      (*I)->setLLVMName(LLVMName);
    }
    // Here we print defs and uses for this instruction
    CnstOperandsList *Defs = (*I)->getDefs(), *Uses = (*I)->getUses();
    bool isCall = (*I)->isCall(), isReturn = (*I)->isReturn(),
         isJump = (*I)->isJump();
    if (Defs->size() > 0 || Uses->size() > 0 || isCall || isReturn) {
      SS << "let ";
      if (isCall) {
	SS << "isCall = 1, ";
      } else if (isJump) {
	SS << "isBranch = 1, isTerminator = 1";
	if (Defs->size() > 0 || Uses->size() > 0)
	  SS << ", ";
      } else if (isReturn) {
	SS << "isReturn = 1, isTerminator = 1";
	if (Defs->size() > 0 || Uses->size() > 0)
	  SS << ", ";
      }
      if (Defs->size() > 0 || isCall) {
	SS << "Defs = [";
	for (CnstOperandsList::const_iterator I2 = Defs->begin(), 
	       E2 = Defs->end(); I2 != E2; ++I2) {
	  if (I2 != Defs->begin())
	    SS << ",";	  
	  SS << (*I2)->getOperandName();      
	}
	if (isCall) {
	  std::list<const Register*>* RList = 
	    RegisterClassManager.getCallerSavedRegs();
	  for (std::list<const Register*>::const_iterator I2 =
	    RList->begin(), E2 = RList->end(); I2 != E2; ++I2) {
	    if (I2 != RList->begin() || Defs->size() != 0)
	      SS << ",";	  
	    SS << (*I2)->getName();
	  }
	  delete RList;
	}
	SS << "]";
	if (Uses->size() > 0 || isCall) SS << ", ";
      }
      if (Uses->size() > 0 || isCall) {
	SS << "Uses = [";
	for (CnstOperandsList::const_iterator I2 = Uses->begin(), 
	       E2 = Uses->end(); I2 != E2; ++I2) {
	  if (I2 != Uses->begin())
	    SS << ",";	  
	  SS << (*I2)->getOperandName();      
	}
	if (isCall) {
	  std::list<CallingConvention*>::const_iterator I2 = RegisterClassManager
	    .getCCBegin();
	  while (I2 != RegisterClassManager.getCCEnd() && 
	    ((*I2)->IsReturnConvention || (*I2)->UseStack)) ++I2;
	  if (I2 != RegisterClassManager.getCCEnd() && (*I2)->getBegin() 
	      != (*I2)->getEnd()) {
	    for (std::list<const Register*>::const_iterator I3 =
	      (*I2)->getBegin(), E3 = (*I2)->getEnd(); I3 != E3; ++I3) {
	      if (I3 != (*I2)->getBegin() || Uses->size() != 0)
		SS << ",";	  
	      SS << (*I3)->getName();
	    }
	  }
	}
	SS << "]";
      }
      SS << " in\n";
    }
    delete Defs;
    delete Uses;
    // No we begin printing the instruction definition per se.
    SS << "def " << (*I)->getLLVMName() << "\t: " << (*I)->getFormat()
      ->getName();
    SS << "<(outs ";
    CnstOperandsList *Ins = (*I)->getIns(), *Outs = (*I)->getOuts();
    // Prints all defined operands (outs)
    for (CnstOperandsList::const_iterator I2 = Outs->begin(),
	   E2 = Outs->end(); I2 != E2; ++I2) {
      if (I2 != Outs->begin())
	SS << ",";
      const RegisterOperand* RO = dynamic_cast<const RegisterOperand*>(*I2);
      // If it is not a register operand, it must be a memory reference
      // FIXME: Enforce this
      if (RO != NULL) {
	SS << RO->getRegisterClass()->getName();
	SS << ":$" << RO->getOperandName();
      } 
    }
    delete Outs;
    SS << "), (ins ";
    // Prints all used operands (ins)
    int DummyIndex = 1;
    for (list<const Operand*>::const_iterator I2 = Ins->begin(),
	   E2 = Ins->end(); I2 != E2; ++I2) {
      if (I2 != Ins->begin())
	SS << ",";
      const RegisterOperand* RO = dynamic_cast<const RegisterOperand*>(*I2);
      // If not a register, treat it as immediate
      if (RO == NULL) {
	// Type 0 is a reserved value for "dummy operand", meaning we don't
	// know what kind this operand is (it is never mentioned in the 
	// semantic tree).
	if ((*I2)->getType() == 0) {
	  // LLVM Operand "tgliteral" is tablegen defined and reserved
	  // for dummies
	  SS << "tgliteral:$dummy" << DummyIndex++;
	} else {
	  SS << InferValueType(*I2) << "imm";
	  SS << ":$" << (*I2)->getOperandName();
	}
	continue;
      } 
      SS << RO->getRegisterClass()->getName();
      SS << ":$" << RO->getOperandName();
    }
    delete Ins;
    SS << "), \"" << (*I)->parseOperandsFmts() << "\", []>;\n";
  }

  return SS.str();
}

// Generate tablegen instructions definitions for XXXInstrInfo.td
string TemplateManager::generateInsnSizeTable() {
  stringstream SS;

  SS << "static unsigned InsnSizeTable[] = {" << endl;
  SS << "0,0,0,0,0,0,0,0,0,0,0,0," << endl;
  for (InstrIterator I = InstructionManager.getBegin(), 
	 E = InstructionManager.getEnd(), Prev = InstructionManager.getEnd();
       I != E; Prev = I++) {
    SS << (*I)->getFormat()->getSizeInBits();    
    if (I != E)
      SS << ",";
  }
  SS << "};" << endl;

  return SS.str();
}

string TemplateManager::generateCallingConventions() {
  stringstream SS;
  
  // First generate all return conventions
  SS << "def RetCC_" << ArchName << ": CallingConv <[\n";
  bool First = true;
  for (list<CallingConvention*>::const_iterator 
	 I = RegisterClassManager.getCCBegin(), 
	 E = RegisterClassManager.getCCEnd(); I != E; ++I) {
    CallingConvention* CC = *I;
    if (!(CC->IsReturnConvention))
      continue; // Not return convention
    if (First) 
      First = false;
    else
      SS << ",";
    if (CC->Type.Type != 0)
      SS << "  CCIfType<[" << InferValueType(&(CC->Type)) << "], ";
    if (CC->UseStack)
      SS << "CCAssignToStack<" << CC->StackSize << "," << CC->StackAlign << ">";
    else {
      SS << "CCAssignToReg<[";
      for (list<const Register*>::const_iterator I2 = CC->getBegin(),
	     E2 = CC->getEnd(); I2 != E2; ++I2) {
	if (I2 != CC->getBegin())
	  SS << ",";
	SS << (*I2)->getName();
      }
      SS << "]>";      
    }
    if (CC->Type.Type != 0)
      SS << ">";
    SS << "\n";
  }
  SS << "]>;\n\n";
  
  // Now generate all calling conventions
  First = true;
  SS << "def CC_" << ArchName << ": CallingConv <[\n";
  for (list<CallingConvention*>::const_iterator 
	 I = RegisterClassManager.getCCBegin(), 
	 E = RegisterClassManager.getCCEnd(); I != E; ++I) {
    CallingConvention* CC = *I;
    if (CC->IsReturnConvention)
      continue; // Not calling convention
    if (First) 
      First = false;
    else
      SS << ",";
    if (CC->Type.Type != 0)
      SS << "  CCIfType<[" << InferValueType(&(CC->Type)) << "], ";
    if (CC->UseStack)
      SS << "CCAssignToStack<" << CC->StackSize << "," << CC->StackAlign << ">";
    else {
      SS << "CCAssignToReg<[";
      for (list<const Register*>::const_iterator I2 = CC->getBegin(),
	     E2 = CC->getEnd(); I2 != E2; ++I2) {
	if (I2 != CC->getBegin())
	  SS << ",";
	SS << (*I2)->getName();
      }
      SS << "]>";      
    }
    if (CC->Type.Type != 0)
      SS << ">";
    SS << "\n";
  }
  SS << "]>;\n\n";
  return SS.str();
}

class UpdateSizeFunctor {
  public:
    bool operator() (Tree* Element) {
      Operand* O = dynamic_cast<Operand *>(Element);
      O->updateSize();
      return true;
    }
};

SearchResult* TemplateManager::FindImplementation(const expression::Tree *Exp,
						  std::ostream &Log) {
  Search S(RuleManager, InstructionManager);
  unsigned SearchDepth = INITIAL_DEPTH;
  SearchResult *R = NULL;
  Tree* _Exp = const_cast<Tree*>(Exp);
  ApplyToLeafs<Tree*,Operator*,UpdateSizeFunctor>(_Exp, UpdateSizeFunctor());
  // Increasing search depth loop - first try with low depth to speed up
  // easy matches
  while (R == NULL || R->Instructions->size() == 0) {
    if (SearchDepth == SEARCH_DEPTH)
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

// REQUIRES: All pattern inference already executed, so the literal map
// is populated.
// This function generates the literal map, so literals get properly printed
// by the backend AsmWritter.
string TemplateManager::generatePrintLiteral() {
  stringstream SS;
  
  SS << "  unsigned index = (unsigned) MI->getOperand(opNum).getImm();\n";
  SS << "  switch(index) {" << endl;
  LMap.printAll(SS);
  SS << "  default: assert (0 && \"Unknown literal index in"
        " AssemblyWritter!\");" << endl;
  SS << "  }" << endl;
  
  return SS.str();
}

// REQUIRES: generateSimplePatterns() must be run before, so the pattern
// inference results (in TemplateManager::InferenceResults) are present.
// Assumptions: "val" and "src" are in the scope where this code will be put, 
//    in which "val" is the frameindex and "src" is the register to be stored.
//    "isKill" is a boolean value in scope, indicating if the src reg is a 
//    kill.
string TemplateManager::generateStoreRegToStackSlot() {
  assert(InferenceResults.StoreToStackSlotSR != NULL &&
         "generateSimplePatterns() must run before");
  //TODO: Differentiate between different src RegisterClasses (in scope as 
  //      const TargetRegisterClass *RC)
  stringstream SS;
  StringMap Defs, ExtraParams;
  Defs["val"] = "FrameIndex";
  Defs["src"] = "Reg";
  ExtraParams["src"] = "false, false, isKill";
  SS << PatTrans.genEmitMI(InferenceResults.StoreToStackSlotSR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 2, &ExtraParams);
  return SS.str();
}

// REQUIRES: generateSimplePatterns() must be run before, so the pattern
// inference results (in TemplateManager::InferenceResults) are present.
string TemplateManager::generateIsStoreToStackSlot() {
  stringstream SS;
  StringMap Defs;
  Defs["val"] = "FrameIndex";
  Defs["src"] = "Reg";
  
  SS << "  int val;" << endl;
  SS << "  unsigned src;" << endl;      
  SS << PatTrans.genIdentifyMI(InferenceResults.StoreToStackSlotSR, Defs,
			       &LMap, "FrameIndex = val;\n"
    "return src;", "pMI");  
  SS << endl;
  return SS.str();
}

// REQUIRES: generateSimplePatterns() must be run before, so the pattern
// inference results (in TemplateManager::InferenceResults) are present.
// Assumptions: "val" and "src" are in the scope where this code will be put, 
//    in which "val" is the frameindex and "src" is the register to be stored.
//    "isKill" is a boolean value in scope, indicating if the src reg is a 
//    kill.
string TemplateManager::generateLoadRegFromStackSlot() {
  assert(InferenceResults.LoadFromStackSlotSR != NULL &&
         "generateSimplePatterns() must run before");
  //TODO: Differentiate between different src RegisterClasses (in scope as 
  //      const TargetRegisterClass *RC)
  stringstream SS;
  StringMap Defs;
  Defs["addr"] = "FrameIndex";
  //Defs["dest"] = "Reg";
  
  SS << PatTrans.genEmitMI(InferenceResults.LoadFromStackSlotSR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 2);
  return SS.str();
}

string TemplateManager::generateTgtImmMask() {
  stringstream SS;
  unsigned size = OperandTable.getType("tgtimm").Size;
  unsigned mask = 0;
  for (unsigned i = 0; i < size; i++) {
    mask = (mask << 1) | 1;
  }
  mask = ~mask;
  SS << mask << "U";
  return SS.str();
}

string TemplateManager::generateFISwitchTo16Bit() {
  assert(InferenceResults.LoadConst16SR != NULL &&
         "generateSimplePatterns() must run before");
  return generateFISwitchToNBit(InferenceResults.LoadConst16SR);
}

string TemplateManager::generateFISwitchTo32Bit() {
  assert(InferenceResults.LoadConst16SR != NULL &&
         "generateSimplePatterns() must run before");
  return generateFISwitchToNBit(InferenceResults.LoadConst32SR);
}

/// This member function generated C++ code to identify a frameindex pattern,
/// whether it is Store or Load, delete it and substitute for another pattern
/// capable of handling large immediates. The param LoadConstPatt should
/// contain the implementation of Constant Load adequate to the number of bits
/// of the immediate.
// Assumptions: bool Patched is in scope, so it can inform if the patch was
//   successful. Input parameters spOffset and AuxReg, containing the large
//   immediate offset and the auxiliar register for base-offset addressing
//   should also be in scope.
string TemplateManager::generateFISwitchToNBit(SearchResult* LoadConstPatt) {
  StringMap Defs;
	 
  stringstream SS;
  // Guess a load
  SS << generateIdent(6) << "unsigned a1 = spOffset;" << endl;
  SS << generateIdent(6) << "unsigned a2;" << endl;
  SS << generateIdent(6) << "bool isLoad = false;" << endl;
  SS << generateIdent(6) << "unsigned dest;" << endl;   
  SS << PatTrans.genFindMIPatRoot(InferenceResults.LoadFromStackSlotSR, "MI",
				  "addr", 6);  
  SS << generateIdent(6) << "if (MIRoot != NULL) {" << endl;
  Defs["addr"] = "FrameIndex";
  Defs["dest"] = "Reg";  
  SS << generateIdent(8) << "int addr;" << endl;  
  SS << PatTrans.genIdentifyMI(InferenceResults.LoadFromStackSlotSR, Defs,
			       &LMap, "isLoad = true; "
			       "I = next(MIGetIterator(MIRoot));",
			       "MIRoot", 8, true); 
  SS << generateIdent(6) << "} // end if (MIRoot != NULL)" << endl;
  SS << generateIdent(6) << "if (isLoad) {" << endl;
  // Emit 16bit const load
  Defs.clear();
  Defs["a1"] = "Imm";   
  //SS << generateIdent(8) << "if (AtBegin) I = MBB.begin(); else I = next(I);"
    // << endl;
  SS << generateIdent(8) << "a2 = " << ArchName << "::" 
     << (*AuxiliarRegs.begin())->getName() << ";" << endl;
  SS << PatTrans.genEmitMI(LoadConstPatt, Defs, &LMap,
			   true, false, &AuxiliarRegs, 8, NULL, "MBB", "I",
			   "TII.get");
  // Emit generic load
  Defs.clear();
  Defs["reg1"] = "Reg";
  Defs["reg2"] = "Reg";
  SS << generateIdent(8) << "unsigned reg1 = AuxReg;" << endl;
  SS << generateIdent(8) << "unsigned reg2 = a2;" << endl;
  SS << PatTrans.genEmitMI(InferenceResults.LoadAddSR, Defs, &LMap, true,
			   false, &AuxiliarRegs, 8, NULL, "MBB", "I",
			   "TII.get");
  SS << generateIdent(8) << "Patched = true;" << endl;
  SS << generateIdent(6) << "} // end if (isLoad)" << endl;  
  SS << generateIdent(6) << "else {" << endl;  
  
  // Guess a store
  SS << generateIdent(8) << "bool idStore = false;" << endl;
  SS << generateIdent(8) << "unsigned src;" << endl;
  SS << PatTrans.genFindMIPatRoot(InferenceResults.StoreToStackSlotSR, "MI",
				  "val", 8);
  SS << generateIdent(8) << "if (MIRoot != NULL) {" << endl;
  Defs.clear();
  Defs["src"] = "Reg";  
  Defs["val"] = "FrameIndex";
  SS << generateIdent(8) << "int val;" << endl;
  SS << PatTrans.genIdentifyMI(InferenceResults.StoreToStackSlotSR, Defs,
			       &LMap, "idStore = true; "
			       "I = next(MIGetIterator(MIRoot));"
			       , "MIRoot", 10, true);
  SS << generateIdent(8) << "} // end if (MIRoot != NULL)" << endl;
  SS << generateIdent(8) << "if (idStore) {" << endl;
  // Emit 16bit const load
  Defs.clear();
  Defs["a1"] = "Imm";
  //SS << generateIdent(10) << "if (AtBegin) I = MBB.begin(); else I = next(I);"
    // << endl;
  SS << generateIdent(10) << "a2 = " << ArchName << "::" 
     << (*AuxiliarRegs.begin())->getName() << ";" << endl;
  SS << PatTrans.genEmitMI(LoadConstPatt, Defs, &LMap,
			   true, false, &AuxiliarRegs, 10, NULL, "MBB", "I",
			   "TII.get");
  // Emit generic store
  Defs.clear();
  Defs["reg1"] = "Reg";
  Defs["reg2"] = "Reg";
  Defs["src"] = "Reg";
  SS << generateIdent(10) << "unsigned reg1 = AuxReg;" << endl;
  SS << generateIdent(10) << "unsigned reg2 = a2;" << endl;
  SS << PatTrans.genEmitMI(InferenceResults.StoreAddSR, Defs, &LMap, true, 
			   false, &AuxiliarRegs, 10, NULL, "MBB", "I",
			   "TII.get");
  SS << generateIdent(10) << "Patched = true;" << endl;
  SS << generateIdent(8) << "} // end if (idStore)" << endl;
  // If it is not load or store, then it is a direct frameindex access
  // This is implemented as a single add operation.
  SS << generateIdent(8) << "else {" << endl;
  SS << generateIdent(10) << "bool isRawFI = false;" << endl;
  SS << generateIdent(10) << "unsigned a3;" << endl;
  SS << generateIdent(10) << "int addr;" << endl;
  SS << PatTrans.genFindMIPatRoot(InferenceResults.FrameIndexSR, "MI",
				  "addr", 10);
  SS << generateIdent(10) << "if (MIRoot != NULL) {" << endl;
  Defs.clear();
  Defs["addr"] = "FrameIndex";  
  Defs["a3"] = "Reg";
  //SS << generateIdent(12) << "int a3;" << endl;
  SS << PatTrans.genIdentifyMI(InferenceResults.FrameIndexSR, Defs,
			       &LMap, "isRawFI = true;"
			       "I = next(MIGetIterator(MIRoot));",
			       "MIRoot", 12, true);
  SS << generateIdent(10) << "} // end if (MIRoot != NULL)" << endl;
  SS << generateIdent(10) << "if (isRawFI) {" << endl;
  // Emit n-bit const load
  Defs.clear();
  Defs["a1"] = "Imm";
  //SS << generateIdent(12) << "if (AtBegin) I = MBB.begin(); else I = next(I);"
    // << endl;
  SS << generateIdent(12) << "a2 = a3;" << endl;
  SS << PatTrans.genEmitMI(LoadConstPatt, Defs, &LMap,
			   true, false, &AuxiliarRegs, 12, NULL, "MBB", "I",
			   "TII.get");  
  SS << generateIdent(12) << "Patched = true;" << endl;
  SS << generateIdent(10) << "} // end if (isRawFI)" << endl;
  SS << generateIdent(8) << "} // end else (idStore)" << endl;
  SS << generateIdent(6) << "} // end else (isLoad)" << endl;  
  return SS.str();
}

// REQUIRES: generateSimplePatterns() must be run before, so the pattern
// inference results (in TemplateManager::InferenceResults) are present.
string TemplateManager::generateIsLoadFromStackSlot() {
  stringstream SS;
  StringMap Defs;
  Defs["addr"] = "FrameIndex";
  Defs["dest"] = "Reg";
  
  SS << "  int addr;" << endl;
  SS << "  unsigned dest;" << endl;      
  SS << PatTrans.genIdentifyMI(InferenceResults.LoadFromStackSlotSR, Defs,
			       &LMap, "FrameIndex = addr;\n"
    "return dest;", "pMI");  
  SS << endl;
  return SS.str();
}

// This function works the inference of stack adjustment for the target
// architecture. It uses FindImplementation(genCopyAddSubImmPat()) for that.
string TemplateManager::generateEliminateCallFramePseudo(std::ostream &Log,
							 bool isPositive) {
  const Register* SP = RegisterClassManager.getStackPointer();
  const RegisterClass* RCSP = RegisterClassManager.getRegRegClass(SP);
  const Tree* Exp = PatternManager::genCopyAddSubImmPat(OperatorTable,
			  OperandTable, RegisterClassManager,
			  isPositive,
			  RCSP->getName(),
			  SP->getName(),
			  RCSP->getName(),
			  SP->getName(),
			  "Size");
  Log << "\nInfering how to adjust stack...\n";
  SearchResult *SR = FindImplementation(Exp, Log);
  if (SR == NULL) {
    Log << "Stack adjustment inference failed. Could not find a instruction\n";
    Log << "sequence to do this in target architecture.\n";
    abort();
  }
  stringstream SS;
  StringMap Defs;
  Defs[SP->getName()] = "Reg";
  Defs["Size"] = "Imm";
  SS << PatTrans.genEmitMI(SR, Defs, &LMap, false, false, &AuxiliarRegs, 4, 
			   NULL, "MBB", "I", "TII.get");
  delete SR;
  delete Exp;
  return SS.str();
}

string TemplateManager::generateAddImm(const string& DestName,
				       const string& BaseName,
				       const string& ImmName,
				       bool Add,
				       unsigned ident) {
  stringstream SS;
  StringMap Defs;
  const unsigned i = ident;
  SS << generateIdent(i) << "if ((" << ImmName << " & " 
     << this->generateTgtImmMask()
     << ") == 0) {" << endl;
  Defs["a1"] = "Reg";
  Defs["a2"] = "Imm";
  SS << generateIdent(i+2) << "unsigned a1 = " << BaseName << ";"
     << endl;
  SS << generateIdent(i+2) << "unsigned a2 = " << ImmName << ";" << endl;
  SS << generateIdent(i+2) << "unsigned a3 = " << DestName << ";" << endl;
  if (Add) {
    SS << PatTrans.genEmitMI(InferenceResults.AddConstSR, Defs, &LMap, true,
			     false, &AuxiliarRegs, i+2,
			    NULL, "MBB", "I", "TII.get");
  } else {
    SS << PatTrans.genEmitMI(InferenceResults.SubConstSR, Defs, &LMap, true,
			     false, &AuxiliarRegs, i+2,
			    NULL, "MBB", "I", "TII.get");
  }
  SS << generateIdent(i) << "} else {" << endl;
  SS << generateIdent(i+2) << "unsigned a1 = " << ImmName << ";" << endl;
  SS << generateIdent(i+2) << "unsigned a2 = " << ArchName << "::" 
     << (*AuxiliarRegs.begin())->getName() << ";" << endl;
  SS << generateIdent(i+2) << "if ((" << ImmName << " & ~0xFFFF) == 0) { //fit"
     " into 16bit imm" << endl;
  Defs.clear();
  Defs["a1"] = "Imm";  
  SS << PatTrans.genEmitMI(InferenceResults.LoadConst16SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, i+4, NULL, "MBB", "I",
			   "TII.get");
  SS << generateIdent(i+2) << "} else { // use 32bit imm" << endl;
  SS << PatTrans.genEmitMI(InferenceResults.LoadConst32SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, i+4, NULL, "MBB", "I",
			   "TII.get");
  SS << generateIdent(i+2) << "}" << endl;
  Defs.clear();  
  Defs["a1"] = "Reg";
  Defs["a2"] = "Reg";
  SS << generateIdent(i+2) << "a1 = " << BaseName << ";"
       << endl;
  SS << generateIdent(i+2) << "unsigned a3 = " << DestName << ";" << endl;
  if (Add) {    
    SS << PatTrans.genEmitMI(InferenceResults.AddSR, Defs, &LMap,
			     true, false, &AuxiliarRegs, i+2, NULL, "MBB", "I",
			     "TII.get");
  } else {
    SS << PatTrans.genEmitMI(InferenceResults.SubSR, Defs, &LMap,
			     true, false, &AuxiliarRegs, i+2, NULL, "MBB", "I",
			     "TII.get");
  }
  SS << generateIdent(i) << "} // end if (" << ImmName << " & ...)" << endl;  
  return SS.str();
}

string TemplateManager::generateStackAction(const string& RegName,
				       const string& OffsetName,
				       bool Store,
				       unsigned ident) {
  stringstream SS;
  StringMap Defs;
  const unsigned i = ident;
  
  SS << generateIdent(i) << "{" << endl;
  SS << generateIdent(i) << "unsigned checkNum;" << endl;
  SS << generateIdent(i) << "bool negative;" << endl;
  SS << generateIdent(i) << "if (" << OffsetName << " < 0) {" << endl;
  SS << generateIdent(i+2) << "checkNum = (unsigned)(0-" << OffsetName 
     << ");" << endl;
  SS << generateIdent(i+2) << "negative =  true;" << endl;
  SS << generateIdent(i) << "} else {" << endl;
  SS << generateIdent(i+2) << "checkNum = (unsigned) " << OffsetName 
     << ";" << endl;
  SS << generateIdent(i+2) << "negative =  false;" << endl;
  SS << generateIdent(i) << "}" << endl;
  SS << generateIdent(i) << "checkNum = checkNum * 2;" << endl;    
  SS << generateIdent(i) << "if ((checkNum & " 
     << this->generateTgtImmMask()
     << ") == 0) {" << endl;
  SS << generateIdent(i+2) << "unsigned a1 = `'__stackpointer_register__`';" << endl;
  SS << generateIdent(i+2) << "int a2 = " << OffsetName << ";" << endl;
  Defs.clear();
  Defs["a1"] = "Reg";
  Defs["a2"] = "Imm";
  if (Store) {
    Defs["src"] = "Reg";
    SS << generateIdent(i+2) << "unsigned src = " << RegName << ";" << endl;
    SS << PatTrans.genEmitMI(InferenceResults.StoreAddConstSR, Defs, &LMap, true,
			   false, &AuxiliarRegs, i+2, NULL, "MBB", "I",
			   "TII.get");
  } else { 
    SS << generateIdent(i+2) << "unsigned dest = " << RegName << ";" << endl;
    SS << PatTrans.genEmitMI(InferenceResults.LoadAddConstSR, Defs, &LMap, true,
			   false, &AuxiliarRegs, i+2, NULL, "MBB", "I",
			   "TII.get");
  }
  SS << generateIdent(i) << "} else {" << endl;
  SS << generateIdent(i+2) << "unsigned a1 = " << OffsetName << ";" << endl;
  SS << generateIdent(i+2) << "unsigned a2 = " << ArchName << "::" 
     << (*AuxiliarRegs.begin())->getName() << ";" << endl;
  SS << generateIdent(i+2) << "if ((" << OffsetName << " & ~0xFFFF) == 0 && !negative) { //fit"
     " into 16bit imm" << endl;
  Defs.clear();
  Defs["a1"] = "Imm";  
  SS << PatTrans.genEmitMI(InferenceResults.LoadConst16SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, i+4, NULL, "MBB", "I",
			   "TII.get");
  SS << generateIdent(i+2) << "} else { // use 32bit imm" << endl;
  SS << PatTrans.genEmitMI(InferenceResults.LoadConst32SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, i+4, NULL, "MBB", "I",
			   "TII.get");
  SS << generateIdent(i+2) << "}" << endl;
  Defs.clear();  
  Defs["reg1"] = "Reg";
  Defs["reg2"] = "Reg";
  SS << generateIdent(i+2) << "unsigned reg1 = `'__stackpointer_register__`';"
       << endl;
  SS << generateIdent(i+2) << "unsigned reg2 = a2;"
       << endl;
  if (Store) {
    Defs["src"] = "Reg";
    SS << generateIdent(i+2) << "unsigned src = " << RegName << ";" << endl;
    SS << PatTrans.genEmitMI(InferenceResults.StoreAddSR, Defs, &LMap,
			   true, false, &AuxiliarRegs, i+2, NULL, "MBB", "I",
			   "TII.get");  
  } else {
    SS << generateIdent(i+2) << "unsigned dest = " << RegName << ";" << endl;
    SS << PatTrans.genEmitMI(InferenceResults.LoadAddSR, Defs, &LMap,
			   true, false, &AuxiliarRegs, i+2, NULL, "MBB", "I",
			   "TII.get");  
  }
  SS << generateIdent(i) << "} // end if (" << OffsetName << "..." <<  endl;
  SS << generateIdent(i) << "}" << endl;
  return SS.str();
}

// This function is responsible for determining the EmitPrologue function
// of the backend.
// Assumptions: "NumBytes" is in the scope where this code will be put, and
//   indicates the stack size, or how much we will adjust the stack.
//   The name of registers SP and FP are also in scope.   
string TemplateManager::generateEmitPrologue(std::ostream &Log) {
  stringstream SS;
  StringMap Defs;

  //
  // Generate instruction to add or subtract stack pointer
  //
  SS << generateAddImm("`'__stackpointer_register__",
		       "`'__stackpointer_register__`'",
		       "NumBytes",
		       RegisterClassManager.getGrowsUp(),
			2);

  
  //
  // ******* Save FP and set it, if necessary **********
  //
  
  SS << generateIdent(2) << "if (hasFP(MF)) {" << endl;  
  
  SS << generateStackAction("`'__frame_register__`'", "FPOffset", true, 4);

  // CONFIGURE FP
  SS << generateAddImm("`'__frame_register__",
		       "`'__stackpointer_register__`'",
		       "NumBytes",
		       !RegisterClassManager.getGrowsUp(),
			4);
  
  SS << generateIdent(2) << "} // end if (hasFP..." <<  endl;
  
  //
  //  ***** RETURN ADDRESS SAVING *****
  //
  SS << generateIdent(2) << "if (MFI->hasCalls()) {" << endl;
  SS << generateStackAction("`'__ra_register__`'", "RAOffset", true, 4);
  SS << generateIdent(2) << "} // end if (MFI->hasCalls()) ... " << endl;
  return SS.str();
}


// This function generates the EmitEpilogue function of the backend.
// Assumptions: The name of registers SP and FP are in scope where this code
//   will be put.
string TemplateManager::generateEmitEpilogue(std::ostream &Log) {
  const Register* SP = RegisterClassManager.getStackPointer();
  const RegisterClass* RCSP = RegisterClassManager.getRegRegClass(SP);
  const Register* FP = RegisterClassManager.getFramePointer();
  const RegisterClass* RCFP = RegisterClassManager.getRegRegClass(FP);
  SearchResult* SR = NULL;
  stringstream SS;
  StringMap Defs;
  
  for (MoveListTy::const_iterator I= InferenceResults.MoveRegToRegList.begin(),
       E = InferenceResults.MoveRegToRegList.end(); I != E; ++I) {
    if (I->first.first == RCFP && I->first.second == RCSP)
      SR = I->second;    
  }
  assert (SR != NULL && "Could not find how to move FP to SP");
  
  SS << generateIdent(2) << "if (hasFP(MF)) {" << endl;  
  Defs["DestReg"] = "Reg";
  Defs["SrcReg"] = "Reg";
  SS << generateIdent(4) << "unsigned DestReg = `'__stackpointer_register__`';"
     << endl;
  SS << generateIdent(4) << "unsigned SrcReg = `'__frame_register__`';"
     << endl;
  SS << PatTrans.genEmitMI(SR, Defs, &LMap, false, false, &AuxiliarRegs, 2,
			   NULL, "MBB", "I", "TII.get");
  // Load FP
  SS << generateStackAction("`'__frame_register__`'", "FPOffset", false, 4);
			   
  SS << generateIdent(2) << "} else { // else if (hasFP(MF))" << endl;
  SS << generateAddImm("`'__stackpointer_register__",
		       "`'__stackpointer_register__`'",
		       "NumBytes",
		       !RegisterClassManager.getGrowsUp(),
			4);			
  SS << generateIdent(2) << "} // end if (hasFP(MF))" << endl;
  
  // Load RA
  SS << generateIdent(2) << "if (MFI->hasCalls()) {" << endl;
  SS << generateStackAction("`'__ra_register__`'", "RAOffset", false, 4);
  SS << generateIdent(2) << "} // end if (MFI->hasCalls()) ... " << endl;
  return SS.str();
}

string TemplateManager::generateSelectCCTGenPatts() {  
  stringstream SS;
  unsigned count = 0;
  
  for (set<RegisterClass*>::const_iterator
	 I = RegisterClassManager.getBegin(),
	 E = RegisterClassManager.getEnd(); I != E; ++I) {    
    //FIXME: Ignore STATUS REGS
    if ((*I)->getNumRegs() < 2)
      continue;
    SS << generateIdent(2) << "def SELECT_CC_" << count << endl;
    SS << generateIdent(2) << " : Pseudo<(outs " << (*I)->getName() 
       << ":$dst), (ins " << (*I)->getName() << ":$T,"
       << (*I)->getName() << ":$F, i32imm:$Cond, " << (*I)->getName() 
       << ":$LHS, " << (*I)->getName() << ":$RHS),\n" 
       << "       \"; SELECT_CC_" << count << " PSEUDO!\","
       << "       [(set " << (*I)->getName() << ":$dst, (selecticc " 
       << (*I)->getName() << ":$T, " << (*I)->getName() << ":$F,\n"
       << "                     imm:$Cond, " << (*I)->getName() << ":$LHS, " 
       << (*I)->getName() << ":$RHS))]>;" << endl;  
    ++count;
  }
  
  return SS.str();
}

// This function generates the XXXInstrInfo::InsertBranch() member function.
// Assumptions: 
//              MachineBasicBlock *tgt (the jump target) is defined with the
//              jump target.
string TemplateManager::generateInsertBranch() {  
  stringstream SS;
  StringMap Defs;
  
  Defs["tgt"] = "MBB";  
  SS << PatTrans.genEmitMI(InferenceResults.UncondBranchSR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 2, NULL, "&MBB", "",
			   "get");
  
  return SS.str();
}

// This function generates the SelectCC pseudo custom inserter, used by LLVM
// scheduler to substitute SelectCC with target instructions and
// a few basicblocks.
// Assumptions: CondCode is an unsigned in scope, indicating what condition      
//              should be used to compare lhs and rhs.
//              TrueVal and FalseVal should also be in scope, as well as
//              MachineBasicBlock *tgt (the jump target).
string TemplateManager::generateEmitSelectCC() {  
  stringstream SS;
  StringMap Defs;
  
  Defs["lhs"] = "Reg";
  Defs["rhs"] = "Reg";
  Defs["tgt"] = "MBB";
  SS << generateIdent(4) << "switch (CondCode) {"<< endl;
  SS << generateIdent(4) << "default: case ISD::SETLT: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond1SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "case ISD::SETGT: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond2SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "case ISD::SETLE: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond3SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "case ISD::SETGE: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond4SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "case ISD::SETULT: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond5SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "case ISD::SETUGT: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond6SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "case ISD::SETUGE: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond7SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "case ISD::SETULE: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond8SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "case ISD::SETNE: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond9SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "case ISD::SETEQ: "<< endl;  
  SS << PatTrans.genEmitMI(InferenceResults.BranchCond10SR, Defs, &LMap,
			   true, false, &AuxiliarRegs, 6, NULL, "BB", "",
			   "TII.get");  
  SS << generateIdent(6) << "break;" << endl;
  SS << generateIdent(4) << "} // end switch (CondCode)"<< endl;
  return SS.str();
}

// This function works out how to transfer between different classes
// of storage in the target architecture and puts this information into
// the backend as c++ code.
string TemplateManager::generateCopyRegPatterns(std::ostream &Log) {
  stringstream SS;
  StringMap Defs;
  Defs["DestReg"] = "Reg";
  Defs["SrcReg"] = "Reg";

  for (set<RegisterClass*>::const_iterator
	 I = RegisterClassManager.getBegin(),
	 E = RegisterClassManager.getEnd(); I != E; ++I) {    
    for (set<RegisterClass*>::const_iterator
	 I2 = RegisterClassManager.getBegin(),
	 E2 = RegisterClassManager.getEnd(); I2 != E2; ++I2) {    
      Log << "\nNow checking if we can copy Registers "
	  << (*I)->getName() << " to " << (*I2)->getName() << "\n";
      const Tree* Exp = PatternManager::genCopyRegToRegPat(OperatorTable,
	  OperandTable, RegisterClassManager, (*I)->getName(), "DestReg", 
	  (*I2)->getName(), "SrcReg");
	if (I == RegisterClassManager.getBegin() 
	    && I2 == RegisterClassManager.getBegin())
	  SS << "  if";
	else
	  SS << "else if";
      SS << " (DestRC == Sparc16::" << (*I)->getName() << "RegisterClass"
         << endl;
      SS << "     && SrcRC == Sparc16::" << (*I2)->getName() 
         << "RegisterClass) {" << endl;
      SearchResult *SR = FindImplementation(Exp, Log);
      if (SR == NULL) {
	SS << "    // Could not infer code to make this transfer" << endl;
	SS << "    return false;" << endl;
      } else {
	InferenceResults.MoveRegToRegList.push_back(make_pair(make_pair(*I,
						      *I2), SR));
	SS << PatTrans.genEmitMI(SR, Defs, &LMap, false, false, &AuxiliarRegs,
				 4);	
      }
      delete Exp;
      SS << "  } ";
    }
  }

  return SS.str();
}

// REQUIRES generateRegCopyPatterns to be previously ran and populate
// InferenceResults.MoveRegToRegList
string TemplateManager::generateIsMove() {
  stringstream SS;
  StringMap Defs;
  Defs["DestReg"] = "Reg";
  Defs["SrcReg"] = "Reg";
  
  SS << "unsigned DestReg;" << endl;
  for (MoveListTy::const_iterator I= InferenceResults.MoveRegToRegList.begin(),
       E = InferenceResults.MoveRegToRegList.end(); I != E; ++I) {
    
    SS << "// SrcRC == " << I->first.first->getName() << " && DestRC == " 
       << I->first.second->getName() << "" << endl;
    SS << PatTrans.genIdentifyMI(I->second, Defs, &LMap, "DstReg = DestReg;\n"
    "return true;", "pMI");    
  }
  SS << endl;
  SS << "return false;";
  return SS.str();
}

// In this member function, a LLVM DAG String (that is, a LLVM SelectionDAG
// to be matched) is postprocessed and its node types may be changed
// according to the target implementation found for this particular DAG.
// Here is the reasoning: If the operand is a Register, then its type
// is copied to the LLVM string, because the LLVM backend knows and recognizes
// with a specific type every register class. But in other cases, the
// opposite happens: the LLVM string defines a type that is copied to the
// pattern implementation node types, because only the LLVM string knows
// better the exact SDNode types used in the LLVM backend.
string 
TemplateManager::PostprocessLLVMDAGString(const string &S, SDNode *DAG) {
  string Result(S);
  // Traverse 
  list<SDNode*> Queue;
  Queue.push_back(DAG);
  while (Queue.size() > 0) {
    SDNode* Element = Queue.front();
    Queue.pop_front();
    // If not leaf, simply add its children without further processing
    if (Element->NumOperands != 0) {
      for (unsigned I = 0, E = Element->NumOperands; I < E; ++I) {
	Queue.push_back(Element->ops[I]);	
      }
      continue;
    }
    // If dummy
    if (Element->LiteralIndex != 0)
      continue;
    // If leaf and not dummy, check the name and, if this name can
    // be found in the LLVM DAG string, change the LLVM node to its type
    // only if it is of register type (TypeName should be empty, otherwise).
    // In case TypeName is empty, do the reverse substitution.
    assert(Element->OpName != NULL && "Leaf SDNode type must have a name");
    string::size_type idx = Result.find(*Element->OpName);
    if (idx == std::string::npos)
	continue;
    string::size_type idx2 = Result.rfind(" ", idx);
    assert(idx2 != 0 && idx2 != string::npos && 
	   "Malformed LLVM DAG String");    
    if (Element->TypeName.size() > 0) {
	Result.replace(idx2+1, idx-3-idx2, Element->TypeName);
	continue;
    } 
    // Element is not a register operand. So we define its typename
    // with the string of the LLVM DAG.
    Element->TypeName = Result.substr(idx2+1, idx-3-idx2);    
  }
  return Result;
}

// Generate the implementation for RETFLAG pattern based on calling
// conventions
string TemplateManager::generateReturnLowering() {  
  stringstream SS;
  std::string InsnName;
  
  for (InstrIterator I = InstructionManager.getBegin(), 
	 E = InstructionManager.getEnd(), Prev = InstructionManager.getEnd();
       I != E; Prev = I++) {
    if ((*I)->isReturn()) {
      InsnName = (*I)->getLLVMName();
      break;
    }
  }
  assert (InsnName.size() > 0 && "Model must have a return instruction");
  
  SS << "  if (Flag.getNode()) {" << endl;
  SS << "    SDValue Ops[] = {Chain, Flag};" << endl;
  SS << "    return SDValue(DAG.getTargetNode(`'__arch__`'::" <<
        InsnName << ", MVT::Other, Ops, 2),0);" << endl;
  SS << "  } else {" << endl;
  SS << "    SDValue Ops[] = {Chain};" << endl;
  SS << "    return SDValue(DAG.getTargetNode(`'__arch__`'::" <<
        InsnName << ", MVT::Other, Ops, 1),0);}" << endl;
  
  return SS.str();
}

// Here we must find the implementation of several simple patterns. For that
// we use the search algorithm.
void TemplateManager::generateSimplePatterns(std::ostream &Log, 					     
			  string **EmitFunctions, string **SwitchCode,
			  string **EmitHeaders) {  
  stringstream SSfunc, SSheaders;
  map<string, MatcherCode> Map;
  SaveAgent Cache(InstructionManager, "cache.file");
  unsigned count = 0;
  bool invalidCache = false;
  std::time_t start, end;
  start = std::time(0);
  Log << "Pattern implementation inference will start now. This may take"
      << " several\nminutes.\n\n";
  if (Cache.CheckVersion() != Version) {
    Cache.ClearFileAndSetVersion(Version);
    invalidCache = true;
  }
#ifdef PARALLEL_SEARCH
#pragma omp parallel for shared(Cache, Log, SSfunc, SSheaders) schedule (dynamic, 1)
#endif
  for (unsigned i = 0; i < PatMan.size(); ++i) {
    PatternManager::Iterator I = PatMan.getElementAt(i);
    bool CacheHit = false;
    SearchResult *SR;
#ifdef PARALLEL_SEARCH
#pragma omp critical
{
#endif    
    count ++;
    Log << "Now finding implementation for : " << I->Name << "\n";  
    SR = invalidCache? NULL : Cache.LoadRecord(I->Name);
    if (SR != NULL) {
      CacheHit = true;
      Log << "Recovered from cache.\n";
      SR->DumpResults(Log);
    }
#ifdef PARALLEL_SEARCH
}
#endif
    if (SR == NULL)
      SR = FindImplementation(I->TargetImpl, Log);
#ifdef PARALLEL_SEARCH
#pragma omp critical
{
#endif
    if (SR == NULL) {
      std::cerr << "Failed: Could not find implementation for pattern " <<
	I->Name << "\n";
      abort();
    }    
    if (!CacheHit)
      Cache.SaveRecord(SR, I->Name);
    SSfunc << PatTrans.genEmitSDNode(SR, I->LLVMDAG, count, &LMap) << endl;
    SSheaders << PatTrans.genEmitSDNodeHeader(count);
    stringstream temp;
    temp << "EmitFunc" << count;
    PatTrans.genSDNodeMatcher(I->LLVMDAG, Map, temp.str());    
    
    // Now check if this is a built-in pattern and we must remember this
    // inference.
    if (I->Name == "STOREFI")
      InferenceResults.StoreToStackSlotSR = SR;
    else if (I->Name == "LOADFI")
      InferenceResults.LoadFromStackSlotSR = SR;
    else if (I->Name == "CONST16")
      InferenceResults.LoadConst16SR = SR;
    else if (I->Name == "CONST32")
      InferenceResults.LoadConst32SR = SR;
    else if (I->Name == "STOREADD")
      InferenceResults.StoreAddSR = SR;
    else if (I->Name == "LOADADD")
      InferenceResults.LoadAddSR = SR;
    else if (I->Name == "ADDCONST")
      InferenceResults.AddConstSR = SR;
    else if (I->Name == "ADD")
      InferenceResults.AddSR = SR;
    else if (I->Name == "SUBCONST")
      InferenceResults.SubConstSR = SR;
    else if (I->Name == "SUB")
      InferenceResults.SubSR = SR;
    else if (I->Name == "FRAMEINDEX")
      InferenceResults.FrameIndexSR = SR;
    else if (I->Name == "STOREADDCONST")
      InferenceResults.StoreAddConstSR = SR;
    else if (I->Name == "LOADADDCONST")
      InferenceResults.LoadAddConstSR = SR;
    else if (I->Name == "BR")
      InferenceResults.UncondBranchSR = SR;
    else if (I->Name == "BRCOND")
      InferenceResults.BranchCond1SR = SR;
    else if (I->Name == "BRCOND2")
      InferenceResults.BranchCond2SR = SR;
    else if (I->Name == "BRCOND3")
      InferenceResults.BranchCond3SR = SR;
    else if (I->Name == "BRCOND4")
      InferenceResults.BranchCond4SR = SR;
    else if (I->Name == "BRCOND5")
      InferenceResults.BranchCond5SR = SR;
    else if (I->Name == "BRCOND6")
      InferenceResults.BranchCond6SR = SR;
    else if (I->Name == "BRCOND7")
      InferenceResults.BranchCond7SR = SR;
    else if (I->Name == "BRCOND8")
      InferenceResults.BranchCond8SR = SR;
    else if (I->Name == "BRCOND9")
      InferenceResults.BranchCond9SR = SR;
    else if (I->Name == "BRCOND10")
      InferenceResults.BranchCond10SR = SR;
    else
      delete SR;
#ifdef PARALLEL_SEARCH
}
#endif
  }    
  stringstream SSswitch;
  for (map<string, MatcherCode>::iterator I = Map.begin(), E = Map.end();
       I != E; ++I) {
    I->second.Print(SSswitch);;
  }
  end = std::time(0);
  Log << count << " pattern(s) implemented successfully in " << 
    std::difftime(end,start) << " second(s).\n";
    
  assert(InferenceResults.StoreToStackSlotSR != NULL && 
	 "Missing built-in pattern STOREFI");
  assert(InferenceResults.LoadFromStackSlotSR != NULL && 
	 "Missing built-in pattern LOADFI");  
  assert(InferenceResults.LoadConst16SR != NULL && 
	 "Missing built-in pattern CONST16");  
  assert(InferenceResults.LoadConst32SR != NULL && 
	 "Missing built-in pattern CONST32");  
  assert(InferenceResults.StoreAddSR != NULL && 
	 "Missing built-in pattern STOREADD");    
  assert(InferenceResults.LoadAddSR != NULL && 
	 "Missing built-in pattern LOADADD");  
  assert(InferenceResults.AddConstSR != NULL && 
	 "Missing built-in pattern ADDCONST");
  assert(InferenceResults.AddSR != NULL && 
	 "Missing built-in pattern ADD");  
  assert(InferenceResults.SubConstSR != NULL && 
	 "Missing built-in pattern SUBCONST");  
  assert(InferenceResults.SubSR != NULL && 
	 "Missing built-in pattern SUB");  
  assert(InferenceResults.FrameIndexSR != NULL && 
	 "Missing built-in pattern FRAMEINDEX");  
  assert(InferenceResults.StoreAddConstSR != NULL && 
	 "Missing built-in pattern STOREADDCONST");  
  assert(InferenceResults.LoadAddConstSR != NULL && 
	 "Missing built-in pattern LOADADDCONST"); 
  assert(InferenceResults.UncondBranchSR != NULL && 
	 "Missing built-in pattern BR");
  assert(InferenceResults.BranchCond1SR != NULL && 
	 "Missing built-in pattern BRCOND");
  assert(InferenceResults.BranchCond2SR != NULL && 
	 "Missing built-in pattern BRCOND2");
  assert(InferenceResults.BranchCond3SR != NULL && 
	 "Missing built-in pattern BRCOND3");
  assert(InferenceResults.BranchCond4SR != NULL && 
	 "Missing built-in pattern BRCOND4");
  assert(InferenceResults.BranchCond5SR != NULL && 
	 "Missing built-in pattern BRCOND5");
  assert(InferenceResults.BranchCond6SR != NULL && 
	 "Missing built-in pattern BRCOND6");
  assert(InferenceResults.BranchCond7SR != NULL && 
	 "Missing built-in pattern BRCOND7");
  assert(InferenceResults.BranchCond8SR != NULL && 
	 "Missing built-in pattern BRCOND8");
  assert(InferenceResults.BranchCond9SR != NULL && 
	 "Missing built-in pattern BRCOND9");
  assert(InferenceResults.BranchCond10SR != NULL && 
	 "Missing built-in pattern BRCOND10");
  //Update output variables
  *EmitFunctions = new std::string(SSfunc.str());
  *SwitchCode = new std::string(SSswitch.str());  
  *EmitHeaders = new std::string(SSheaders.str());
}

// Creates LLVM backend files based on template files feeded with
// target specific data.
void TemplateManager::CreateBackendFiles()
{
  int ret;
  std::string MacroFileName = WorkingDir;
  std::string RegisterInfoIn = "./template/XXXRegisterInfo.td";
  std::string RegisterInfoOut = WorkingDir;
  std::string RegisterInfo2In = "./template/XXXRegisterInfo.cpp";
  std::string RegisterInfo2Out = WorkingDir;
  std::string RegisterInfo3In = "./template/XXXRegisterInfo.h";
  std::string RegisterInfo3Out = WorkingDir;
  std::string InstrInfoIn = "./template/XXXInstrInfo.td";
  std::string InstrInfoOut = WorkingDir;
  std::string InstrInfo2In = "./template/XXXInstrInfo.cpp";
  std::string InstrInfo2Out = WorkingDir;
  std::string InstrInfo3In = "./template/XXXInstrInfo.h";
  std::string InstrInfo3Out = WorkingDir;
  std::string ISelDAGToDAGIn = "./template/XXXISelDAGToDAG.cpp";
  std::string ISelDAGToDAGOut = WorkingDir;
  std::string ISelLoweringIn = "./template/XXXISelLowering.cpp";
  std::string ISelLoweringOut = WorkingDir;
  std::string ISelLowering2In = "./template/XXXISelLowering.h";
  std::string ISelLowering2Out = WorkingDir;
  std::string TargetIn = "./template/XXX.td";
  std::string TargetOut = WorkingDir;
  std::string Target2In = "./template/XXX.h";
  std::string Target2Out = WorkingDir;
  std::string SubtargetIn = "./template/XXXSubtarget.cpp";
  std::string SubtargetOut = WorkingDir;
  std::string Subtarget2In = "./template/XXXSubtarget.h";
  std::string Subtarget2Out = WorkingDir;
  std::string TargetMachineIn = "./template/XXXTargetMachine.cpp";
  std::string TargetMachineOut = WorkingDir;
  std::string TargetMachine2In = "./template/XXXTargetMachine.h";
  std::string TargetMachine2Out = WorkingDir;
  std::string TargetAsmInfoIn = "./template/XXXTargetAsmInfo.cpp";
  std::string TargetAsmInfoOut = WorkingDir;
  std::string TargetAsmInfo2In = "./template/XXXTargetAsmInfo.h";
  std::string TargetAsmInfo2Out = WorkingDir;
  std::string AsmPrinterIn = "./template/XXXAsmPrinter.cpp";
  std::string AsmPrinterOut = WorkingDir;
  std::string CallingConvIn = "./template/XXXCallingConv.td";
  std::string CallingConvOut = WorkingDir;
  std::string MachineFunctionIn = "./template/XXXMachineFunction.h";
  std::string MachineFunctionOut = WorkingDir;
  std::string CMakeListsIn = "./template/CMakeLists.txt";
  std::string CMakeListsOut = WorkingDir;
  std::string MakefileIn = "./template/Makefile";
  std::string MakefileOut = WorkingDir; 

  MacroFileName += "/macro.m4";
  RegisterInfoOut += "/";
  RegisterInfoOut += ArchName;
  RegisterInfoOut += "RegisterInfo.td";
  RegisterInfo2Out += "/";
  RegisterInfo2Out += ArchName;
  RegisterInfo2Out += "RegisterInfo.cpp";
  RegisterInfo3Out += "/";
  RegisterInfo3Out += ArchName;
  RegisterInfo3Out += "RegisterInfo.h";
  InstrInfoOut += "/";
  InstrInfoOut += ArchName;
  InstrInfoOut += "InstrInfo.td";
  InstrInfo2Out += "/";
  InstrInfo2Out += ArchName;
  InstrInfo2Out += "InstrInfo.cpp";
  InstrInfo3Out += "/";
  InstrInfo3Out += ArchName;
  InstrInfo3Out += "InstrInfo.h";
  ISelDAGToDAGOut += "/";
  ISelDAGToDAGOut += ArchName;
  ISelDAGToDAGOut += "ISelDAGToDAG.cpp";
  ISelLoweringOut += "/";
  ISelLoweringOut += ArchName;
  ISelLoweringOut += "ISelLowering.cpp";
  ISelLowering2Out += "/";
  ISelLowering2Out += ArchName;
  ISelLowering2Out += "ISelLowering.h";
  CallingConvOut += "/";
  CallingConvOut += ArchName;
  CallingConvOut += "CallingConv.td";
  TargetOut += "/";
  TargetOut += ArchName;
  TargetOut += ".td";
  Target2Out += "/";
  Target2Out += ArchName;
  Target2Out += ".h";
  SubtargetOut += "/";
  SubtargetOut += ArchName;
  SubtargetOut += "Subtarget.cpp";
  Subtarget2Out += "/";
  Subtarget2Out += ArchName;
  Subtarget2Out += "Subtarget.h";
  TargetMachineOut += "/";
  TargetMachineOut += ArchName;
  TargetMachineOut += "TargetMachine.cpp";
  TargetMachine2Out += "/";
  TargetMachine2Out += ArchName;
  TargetMachine2Out += "TargetMachine.h";
  TargetAsmInfoOut += "/";
  TargetAsmInfoOut += ArchName;
  TargetAsmInfoOut += "TargetAsmInfo.cpp";
  TargetAsmInfo2Out += "/";
  TargetAsmInfo2Out += ArchName;
  TargetAsmInfo2Out += "TargetAsmInfo.h";
  AsmPrinterOut += "/";
  AsmPrinterOut += ArchName;
  AsmPrinterOut += "AsmPrinter.cpp";
  MachineFunctionOut += "/";
  MachineFunctionOut += ArchName;
  MachineFunctionOut += "MachineFunction.h";

  CMakeListsOut += "/CMakeLists.txt";
  MakefileOut += "/Makefile";

  // First creates our macro file to insert target specific data
  // into templates
  CreateM4File();

  // Creates XXXRegisterInfo.td, h, cpp files
  ret = system(("m4 -P " + MacroFileName + " " + RegisterInfoIn +
		" > " + RegisterInfoOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXRegisterInfo.td\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + RegisterInfo2In +
		" > " + RegisterInfo2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXRegisterInfo.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + RegisterInfo3In +
		" > " + RegisterInfo3Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXRegisterInfo.h\n";
    exit(1);
  }

  // Creates XXXInstrInfo.td, h, cpp files
  ret = system(("m4 -P " + MacroFileName + " " + InstrInfoIn +
		" > " + InstrInfoOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXInstrInfo.td\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + InstrInfo2In +
		" > " + InstrInfo2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXInstrInfo.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + InstrInfo3In +
		" > " + InstrInfo3Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXInstrInfo.h\n";
    exit(1);
  }
  // Creates XXXISelDAGToDAG.cpp file
  ret = system(("m4 -P " + MacroFileName + " " + ISelDAGToDAGIn +
		" > " + ISelDAGToDAGOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXISelDAGToDAG.cpp\n";
    exit(1);
  }

  // Creates XXXISelLowering.cpp and h files
  ret = system(("m4 -P " + MacroFileName + " " + ISelLoweringIn +
		" > " + ISelLoweringOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXISelLowering.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + ISelLowering2In +
		" > " + ISelLowering2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXISelLowering.h\n";
    exit(1);
  }

  // Creates XXXCallingConv.td file
  ret = system(("m4 -P " + MacroFileName + " " + CallingConvIn +
		" > " + CallingConvOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXCallingConv.td\n";
    exit(1);
  }

  // Creates XXX.td and files
  ret = system(("m4 -P " + MacroFileName + " " + TargetIn +
		" > " + TargetOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXX.td\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + Target2In +
		" > " + Target2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXX.h\n";
    exit(1);
  }

  // Creates XXXSubtarget.cpp and h files
  ret = system(("m4 -P " + MacroFileName + " " + SubtargetIn +
		" > " + SubtargetOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXSubtarget.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + Subtarget2In +
		" > " + Subtarget2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXSubtarget.h\n";
    exit(1);
  }

  // Creates XXXTargetMachine.cpp and h files
  ret = system(("m4 -P " + MacroFileName + " " + TargetMachineIn +
		" > " + TargetMachineOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXTargetMachine.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + TargetMachine2In +
		" > " + TargetMachine2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXTargetMachine.h\n";
    exit(1);
  }

  // Creates XXXTargetAsmInfo.cpp and h files
  ret = system(("m4 -P " + MacroFileName + " " + TargetAsmInfoIn +
		" > " + TargetAsmInfoOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXTargetAsmInfo.cpp\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + TargetAsmInfo2In +
		" > " + TargetAsmInfo2Out).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXTargetAsmInfo.h\n";
    exit(1);
  }

  // Creates XXXAsmPrinter.cpp file
  ret = system(("m4 -P " + MacroFileName + " " + AsmPrinterIn +
		" > " + AsmPrinterOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXAsmPrinter.cpp\n";
    exit(1);
  }

  // Creates XXXMachineFunction.h file
  ret = system(("m4 -P " + MacroFileName + " " + MachineFunctionIn +
		" > " + MachineFunctionOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo XXXMachineFunction.h\n";
    exit(1);
  }

  // Copy CMakeLists and Makefile
  ret = system(("m4 -P " + MacroFileName + " " + CMakeListsIn +
		" > " + CMakeListsOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo CMakeLists.txt\n";
    exit(1);
  }
  ret = system(("m4 -P " + MacroFileName + " " + MakefileIn +
		" > " + MakefileOut).c_str());
  if (WEXITSTATUS(ret) != 0) {
    std::cout << "Erro ao criar arquivo Makefile\n";
    exit(1);
  }

  // Success
  remove(MacroFileName.c_str());
}
