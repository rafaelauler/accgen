//===- genllvbe.cpp - Main implementatino file             --*- C++ -*-----===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <fstream>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "InsnFormat.h"
#include "Instruction.h"
#include "ArchEmitter.h"
#include "TemplateManager.h"
#include "CMemWatcher.h"
#include "InsnSelector/Semantic.h"
#include <map>

extern "C" { 
  #include "acpp.h"
  extern char * ac_asm_get_comment_chars() ;
}

// Some parser dependent data and code
extern FILE* yybein;
extern int yybeparse();

// Defined in semantics parser 
extern backendgen::InstrManager InstructionManager;
extern backendgen::expression::OperandTableManager OperandTable;
extern backendgen::expression::OperatorTableManager OperatorTable;
extern backendgen::TransformationRules RuleManager;
extern backendgen::expression::RegClassManager RegisterManager;
extern backendgen::expression::PatternManager PatMan;

char *file_name = NULL; /* name of the main ArchC file */
char *isa_filename_with_path = NULL;

using namespace backendgen;

// Function responsible for parsing backend generation information
// available in FileName
void ParseBackendInformation(const char* FileName) {
  yybein = fopen(FileName, "r");
  if (yybein == NULL) { 
    std::cerr << "Could not open backend information file\"" << FileName 
	   << "\".\n";
    exit(EXIT_FAILURE);
  }
  yybeparse();
}

void parse_archc_description(char **argv) {
  /* Initializes the pre-processor */
  acppInit(1);
  file_name = (char *)malloc(strlen(argv[1]) + strlen(argv[2]) + 1);
  strcpy(file_name, argv[1]);
  strcpy(file_name + strlen(argv[1]), argv[2]);
  
  /* Parse the ARCH file */
  if (!acppLoad(file_name)) {
    fprintf(stderr, "Invalid file: '%s'.\n", file_name);
    exit(1);
  }

  if (acppRun()) {
    fprintf(stderr, "Parser error in ARCH file.\n");
    exit(1);
  }
  acppUnload();

  /* Parse the ISA file */
  isa_filename_with_path = (char *)malloc(strlen(argv[1]) + strlen(isa_filename)
					  + 1);
  strcpy(isa_filename_with_path, argv[1]);
  strcpy(isa_filename_with_path + strlen(argv[1]), isa_filename);
  
  if (!acppLoad(isa_filename_with_path)) {
    
    fprintf(stderr, "Invalid ISA file: '%s'.\n", isa_filename);
    exit(1);
  }
  if (acppRun()) {
    fprintf(stderr, "Parser error in ISA file.\n");
    exit(1);
  }
  acppUnload();

  free(file_name);
  free(isa_filename_with_path);
}

void print_formats() {
  ac_dec_format* pformat;
  ac_dec_field* pfield;
  for (pformat = format_ins_list; pformat != NULL; pformat = pformat->next) {
    std::cout << pformat->name << ", size:" << pformat->size << std::endl;
    for (pfield = pformat->fields; pfield != NULL; pfield = pfield->next) {
      std::cout << "  " << pfield->name 
                << ", id:" << pfield->id
                << ", size:" << pfield->size
                << ", first_bit:" << pfield->first_bit
                << std::endl;
    }
  }
}

std::map<std::string, InsnFormat*> FormatMap;
typedef std::map<std::string, InsnFormat*>::iterator FormatMapIter;

void DebugFormats() {
  for (FormatMapIter I = FormatMap.begin(), E = FormatMap.end(); I != E; ++I) {
    InsnFormat *IF = I->second;
    std::cout << IF->getName() 
              << ", size:" << IF->getSizeInBits() 
              << ", baseclass: " << IF->getBaseLLVMFormatClassName()
              << std::endl;
    std::vector<FormatField *> FF = IF->getFields();
    for (std::vector<FormatField *>::iterator FI = FF.begin(), FE = FF.end(); 
         FI != FE; ++FI) {
      FormatField *Field = *FI;
      std::cout << "  " <<  Field->getName() 
                << ", size:" << Field->getSizeInBits()
                << ", (" << Field->getStartBitPos()
                << "," << Field->getEndBitPos() << ")"
                << ", grpnum=" << Field->getGroupId()
                << ","
                << std::endl;
    }
  }
}

std::vector<InsnFormat *> BaseFormatNames;

void BuildFormats() {
  ac_dec_format* pformat;
  ac_dec_field* pfield;

  // Fulfill all instruction formats with their fields.
  for (pformat = format_ins_list; pformat != NULL; pformat = pformat->next) {
    InsnFormat *IF = new InsnFormat(pformat->name, pformat->size);
    unsigned startbit, endbit;
    for (pfield = pformat->fields; pfield != NULL; pfield = pfield->next) {
      // Calculation adequate for both endianess
      endbit = pformat->size - pfield->first_bit - 1;
      startbit = endbit + pfield->size - 1;
      IF->addField(new FormatField(pfield->name, pfield->size, startbit, endbit));
    }
    FormatMap[pformat->name] = IF;
  }

  // Only looks into the first field to create a base
  // class for that kind of format. This could be improved to detect as much
  // as possible for each format. That is, all insn formats with the first 
  // field "op:5" will belong to format class A, and those with "opext:5" to
  // class B. An architecture usually has only one base class.
  std::vector<const char *> FirstFields;
  for (FormatMapIter I = FormatMap.begin(), E = FormatMap.end(); I != E; ++I) {
    InsnFormat *IF = I->second;
    bool HasEntry = false;

    for (int i = 0, e = FirstFields.size(); i != e; ++i) {
      if (!strcmp(IF->getField(0)->getName(), FirstFields[i])) {
        HasEntry = true;
        IF->setBaseLLVMFormatClassName(i);
      }
    }

    if (!HasEntry) {
      FirstFields.push_back(IF->getField(0)->getName());
      IF->setBaseLLVMFormatClassName(FirstFields.size()-1);
      BaseFormatNames.push_back(IF);
    }

    IF->recognizeGroups();
  }

  //DebugFormats();
}

void DeallocateFormats() {
  for (FormatMapIter I = FormatMap.begin(), E = FormatMap.end(); I != E; ++I) {
    delete I->second;
  }
}

InsnIdMapTy InsnIdMap;

void DebugInsn() {
  for (InsnIdMapIter IM = InsnIdMap.begin(), EM = InsnIdMap.end(); IM != EM; 
       ++IM) {
    std::vector<Instruction *> VI = IM->second;
    for (std::vector<Instruction *>::iterator II = VI.begin(), IE = VI.end(); 
	 II != IE; ++II) {
      Instruction *I = *II;
      std::cout << I->getName()
                << ", " << IM->first
                << ", " << I->getFormat()->getName()
                << ", " << I->getOperandsFmts()
                << " "; //std::endl;
      //std::cout << I->getFmtStr();
      std::cout << std::endl;
      for (int i = 0, e = I->getNumOperands(); i != e; ++i) {
        std::cout << "  " << I->getOperand(i)->getName() << "(";
        for (int fi = 0, fe = I->getOperand(i)->getNumFields(); fi != fe; ++fi)
	  { 
	    FormatField *FF = I->getOperand(i)->getField(fi);
	    std::cout << FF->getName()
		      << ":" << FF->getSizeInBits()
		      << ",";
	  }
        std::cout << ")" << std::endl;
      }
    }
  }
}

void BuildInsn() {
  ac_asm_insn *pinsn = ac_asm_get_asm_insn_list();
  ac_operand_list *operand;
  ac_asm_insn_field *field;

  for (; pinsn != NULL; pinsn = pinsn->next) {

    if (pinsn->insn == NULL)
      continue; // Pseudo!

    Instruction *I = new Instruction(pinsn->insn->name, 
				     pinsn->op_literal, 
				     FormatMap[pinsn->insn->format],
				     pinsn->mnemonic);

    unsigned startbit, endbit;
    for (operand = pinsn->operands; operand != NULL; operand = operand->next) {
      InsnOperand *IO = new InsnOperand(operand->str);
      for (field = operand->fields; field != NULL; field = field->next) {
        endbit = pinsn->insn->size - field->first_bit - 1;
        startbit = endbit + field->size - 1;
        IO->addField(new FormatField(field->name, field->size, startbit, 
				     endbit));
      }
      I->addOperand(IO);
    }
    //I->parseOperandsFmts();

    //if (InsnIdMap.find(pinsn->insn->id) == InsnIdMap.end())
    InsnIdMap[pinsn->insn->id].push_back(I);
    InstructionManager.addInstruction(I);
  }
  InstructionManager.SortInstructions();

  //DebugInsn();
}

void print_insns() {
  ac_asm_insn *pinsn = ac_asm_get_asm_insn_list();
  ac_operand_list *operand;
  ac_asm_insn_field *field;

  for (; pinsn != NULL; pinsn = pinsn->next) {
    if (pinsn->insn == NULL) {
      std::cout << pinsn->mnemonic
		<< ", * pseudo *" 
      		<< ", " << pinsn->op_literal
		<< std::endl;  
    } else {
      std::cout << pinsn->mnemonic
		<< ", " << pinsn->insn->id 
		<< ", " << pinsn->insn->format 
		<< ", " << pinsn->op_literal
		<< std::endl;  
    }
    for (operand = pinsn->operands; operand != NULL; operand = operand->next) {
      std::cout << "  " << operand->str << "(";
      for (field = operand->fields; field != NULL; field = field->next) {
        std::cout << field->name 
        << ":" << field->size 
        << "," << field->id << ",";
      }
      std::cout << ")" << std::endl;
    }
  }
}

// FIXME: This function seems to propose an impossible task
void DeallocateACParser() {
  ac_asm_insn *pinsn = ac_asm_get_asm_insn_list(), *ppinsn;
  ac_operand_list *operand, *poperand;
  ac_asm_insn_field *field, *pfield;
  for (; pinsn != NULL; pinsn = ppinsn) {    
    free(pinsn->mnemonic);
    free(pinsn->op_literal);
    for (operand = pinsn->operands; operand != NULL; operand = poperand) {
      free(operand->str);
      for (field = operand->fields; field != NULL; field = pfield) {
	free(field->name);
	pfield = field->next;
	free(field);
      }
      poperand = operand->next;
      free(operand);
    }
    ppinsn = pinsn->next;
    free(pinsn);
  }
  ac_dec_format* pformat, *ppformat;
  ac_dec_field* ofield, *pofield;
  for (pformat = format_ins_list; pformat != NULL; pformat = ppformat) {
    free(pformat->name);
    for (ofield = pformat->fields; ofield != NULL; ofield = pofield) {
      free(ofield->name);
      pofield = ofield->next;
      free(ofield);
    }
    ppformat = pformat->next;
    free(pformat);
  }
  for (pformat = format_reg_list; pformat != NULL; pformat = ppformat) {
    free(pformat->name);
    for (ofield = pformat->fields; ofield != NULL; ofield = pofield) {
      free(ofield->name);
      pofield = ofield->next;
      free(ofield);
    }
    ppformat = pformat->next;
    free(pformat);
  }
  ac_dec_instr *pinstr, *ppinstr;
  for (pinstr = instr_list; pinstr != NULL; pinstr = ppinstr) {
    free(pinstr->name);
    //free(pinstr->mnemonic);
    //free(pinstr->asm_str);
    free(pinstr->format);
    //ac_dec_list *plist, *pplist;
    //for (plist = pinstr->dec_list; plist != NULL && plist != (ac_dec_list*)1;
    //	 plist = pplist) {
    //  free(plist->name);
    //  pplist = plist->next;
    //  free(plist);
    //}
    ppinstr = pinstr->next;
    free(pinstr);
  }
}

void create_dir(const char *path) {
  struct stat sb;
  if (stat(path, &sb) == -1) {
    mkdir(path, 0777); 
  }
}

int main(int argc, char **argv) {
  std::cout << "ArchC LLVM Backend Generator (version 0.1).\n";
  if (argc != 4) {
    std::cerr << "Wrong number of parameters.\n";
    std::cerr << "Usage is: " << argv[0] << " <project folder> <project file>"
	      << " <backend info file>\n\n";
    std::cerr << "Example: " << argv[0] << " /l/ArchC/ARM/ arm.ac" 
	      << "arm-be.ac\n\n";
    exit(EXIT_FAILURE);
  }

  helper::CMemWatcher *MemWatcher = helper::CMemWatcher::Instance();
  // FIXME: Temporary fix for myriad of leaks. DeallocateACParser()
  // should be used instead.
  MemWatcher->InstallHooks();
  parse_archc_description(argv);
  MemWatcher->UninstallHooks();
  //print_formats();  
  //print_insns();
  const char *TmpDir = "llvmbackend";
  create_dir(TmpDir);

  // Build information needed to parse backend generation file
  BuildFormats();
  BuildInsn();  
  ParseBackendInformation(argv[3]);

  // Create the LLVM Instruction Formats file for Sparc16
  const char *FormatsFile = "llvmbackend/Sparc16InstrFormats.td";
  std::ofstream O;
  ArchEmitter AEmitter = ArchEmitter();
  O.open(FormatsFile, std::ios::out | std::ios::trunc); 
  AEmitter.EmitInstrutionFormatClasses(FormatMap, BaseFormatNames, O, 
				       "Sparc16");
  O.close();

  // Create LLVM backend files based on template files
  TemplateManager TM(RuleManager, InstructionManager, RegisterManager,
		     OperandTable, OperatorTable, PatMan);
  TM.SetArchName("sparc16");
  TM.SetCommentChar(ac_asm_get_comment_chars()[0]);
  TM.SetNumRegs(48);
  TM.SetWorkingDir(TmpDir);
  TM.SetIsBigEndian(ac_tgt_endian == 1? true: false);
  TM.SetWordSize(wordsize);
  TM.CreateBackendFiles();

  DeallocateFormats();
  MemWatcher->ReportStatistics(std::cout);
  MemWatcher->FreeAll();
  helper::CMemWatcher::Destroy();
  //  DeallocateACParser();
  
  return 0;
}
