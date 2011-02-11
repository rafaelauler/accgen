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
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#include <cctype>
#include <cstdio>

#include "InsnFormat.h"
#include "Instruction.h"
#include "ArchEmitter.h"
#include "TemplateManager.h"
#include "CMemWatcher.h"
#include "SaveAgent.h"
#include "AsmProfileGen.h"
#include "InsnSelector/Semantic.h"
#include <map>

#include <boost/regex.hpp>

extern "C" { 
  #include "acpp.h"
  extern char * ac_asm_get_comment_chars() ;
}

// Some parser dependent data and code
extern FILE* yybein;
extern int yybeparse();
extern bool HasError;
extern unsigned RulesNumLines;

// Defined in semantics parser 
extern backendgen::InstrManager InstructionManager;
extern backendgen::expression::OperandTableManager OperandTable;
extern backendgen::expression::OperatorTableManager OperatorTable;
extern backendgen::TransformationRules RuleManager;
extern backendgen::expression::RegClassManager RegisterManager;
extern backendgen::expression::PatternManager PatMan;

using namespace backendgen;
using std::string;

namespace {

struct StartupInfo {
  string ProjectFolder;
  string ProjectFile;
  string BackendFile;
  string RulesFile;
  string TemplateDir;
  string LLVMDir;
  string ArchName;
  string ISAFilename;
  bool ForceCacheFlag;
  bool GenerateBackendFlag;
  bool GeneratePatternsFlag;
  bool GenerateProfilingFlag;
  bool VerboseFlag;
  StartupInfo() {
    ForceCacheFlag = false;
    VerboseFlag = false;
    GenerateBackendFlag = false;
    GeneratePatternsFlag = false;
    GenerateProfilingFlag = false;
  }
};

}

inline bool CopyFile(FILE *dst, FILE *src) {
  unsigned char *buf = (unsigned char*) malloc(sizeof(unsigned char)*100);
  size_t num = 0;
  while ((num = std::fread(buf, sizeof(unsigned char), 100, src)) > 0) {
    if (std::fwrite(buf, sizeof(unsigned char), num, dst) != num) {
      std::cerr << "Error writing to temporary file.\n";
      free(buf);
      return false;
    }
  }
  free(buf);
  return true;
}

inline unsigned CountNumLines(FILE *fp) {
  int c;
  unsigned count = 0;
  while ((c = std::fgetc(fp)) != EOF) {
    if (c == '\n')
      ++count;
  }
  std::rewind(fp);
  return count;
}

// Function responsible for parsing backend generation information
// available in FileName
bool ParseBackendInformation(const char* RuleFileName, const char* BackendFileName) {
  FILE *fp = std::tmpfile();
  FILE *rfp = std::fopen(RuleFileName, "r");
  FILE *bfp = std::fopen(BackendFileName, "r");
  
  RulesNumLines = CountNumLines(rfp);

  if (rfp == NULL) { 
    std::cerr << "Could not open rule information file \"" << RuleFileName 
	   << "\".\n";
    exit(EXIT_FAILURE);
  }
  if (bfp == NULL) { 
    std::cerr << "Could not open backend information file \"" << BackendFileName 
	   << "\".\n";
    exit(EXIT_FAILURE);
  }
  
  CopyFile(fp, rfp);
  CopyFile(fp, bfp);
  
  std::fclose(rfp);
  std::fclose(bfp);
  std::rewind(fp);
  
  yybein = fp;  
  yybeparse();
  
  std::fclose(fp);  
  
  return !HasError;
}

void parse_archc_description(StartupInfo* SI) {
  /* Initializes the pre-processor */
  acppInit(1);
  
  string fn = SI->ProjectFolder;
  fn += '/';
  fn.append(SI->ProjectFile);  
  
  char *tempfn = strdup(fn.c_str());  
  
  /* Parse the ARCH file */
  if (!acppLoad(tempfn)) {
    std::cerr << "Invalid main model file: '" << fn << "'.\n";
    free(tempfn);
    exit(EXIT_FAILURE);
  }
  free(tempfn);

  if (acppRun()) {
    std::cerr << "Parser error in ARCH file.\n";
    exit(EXIT_FAILURE);
  }
  acppUnload();

  /* Parse the ISA file */
  SI->ISAFilename = SI->ProjectFolder;
  SI->ISAFilename += '/';
  SI->ISAFilename.append(isa_filename);
    
  tempfn = strdup(SI->ISAFilename.c_str());
  
  if (!acppLoad(tempfn)) {
    std::cerr << "Invalid ISA file: '" << SI->ISAFilename << "'.\n";
    free(tempfn);
    exit(EXIT_FAILURE);
  }
  free(tempfn);
  
  if (acppRun()) {
    std::cerr << "Parser error in ISA file.\n";
    exit(EXIT_FAILURE);
  }
  acppUnload();
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

void BuildInsn(bool Debug) {
  ac_asm_insn *pinsn = ac_asm_get_asm_insn_list();
  ac_operand_list *operand;
  ac_asm_insn_field *field;

  for (; pinsn != NULL; pinsn = pinsn->next) {

    if (pinsn->insn == NULL)
      continue; // Pseudo!

    Instruction *I = new Instruction(pinsn->insn->name, 
				     pinsn->op_literal_unformatted, 
				     FormatMap[pinsn->insn->format],
				     pinsn->mnemonic);    
    //std::cerr << pinsn->insn->name << " ";
    //std::cerr << pinsn->op_literal_unformatted << "\n";
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
  InstructionManager.SetLLVMNames();
  if (Debug)
    DebugInsn();
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

void PrintUsage(string AppName, string Message) {
  std::cerr << Message;
  std::cerr << "Usage is: " << AppName << " <project file>"
	    << " [flags]\n\n";
  std::cerr << "Example: " << AppName << " armv5e.ac\n\n";
}

string ExtractArchName(string ProjectFileName) {
  unsigned i;
  for (i = 0; i < ProjectFileName.size(); ++i) {
    if (ProjectFileName[i] == '.')
      break;
  }
  assert (i < ProjectFileName.size() && 
          "Inconsistent project file name - missing extension");
  string ArchName = ProjectFileName.substr(0,i);
  return ArchName;
}

StartupInfo * ProcessArguments(int argc, char **argv) {
  StartupInfo *Result = new StartupInfo();  
  char *cur_dir = (char *) malloc(sizeof(char)*200);
  // Process implicit parameters
  if (!getcwd(cur_dir, 200)) {
    perror("getcwd failed");
    exit(EXIT_FAILURE);
  }
  Result->ProjectFolder = cur_dir;
  free(cur_dir);
  Result->BackendFile = "compiler_info.ac";
  
  // Process input parameters 
  if (argc == 1) {
    PrintUsage(argv[0], "Expecting model file name.\n");
    delete Result;
    return NULL;
  }
  int num = argc;
  do {
    --num;
    // Special case, expecting project file (model.ac)
    if (num == 1) {
      Result->ProjectFile = argv[num];
      if (Result->ProjectFile[0] == '-') {
	PrintUsage(argv[0], "Unexpected flag. Expecting project file name.\n");
	delete Result;
	return NULL;
      }
      continue;
    }
    // Process flag
    string Param(argv[num]);
    if (Param[0] != '-') {
      PrintUsage(argv[0], "Expecting flag.\n");
      delete Result;
      return NULL;
    }
    char c = Param[1];
    switch(c) {
      default:
	PrintUsage(argv[0], "Unrecognized flag.\n");
	delete Result;
	return NULL;
      case 'f':
	std::cout << "Force cache usage flag used.\n";
	Result->ForceCacheFlag = true;
	break;
      case 'v':
	std::cout << "Verbose flag used.\n";
	Result->VerboseFlag = true;
	break;
      case 't':	
	std::cout << "Generate patterns mode selected.\n";
	Result->GeneratePatternsFlag = true;
	assert(Result->GenerateBackendFlag == false && 
	       Result->GenerateProfilingFlag == false &&
	       "Only one mode can be selected.");
	break;
      case 'p':
	std::cout << "Generate assembly profile mode selected.\n";
	Result->GenerateProfilingFlag = true;
	assert(Result->GenerateBackendFlag == false && 
	       Result->GeneratePatternsFlag == false &&
	       "Only one mode can be selected.");
	break;
      case 'b':
	std::cout << "Generate compiler backend mode selected.\n";
	Result->GenerateBackendFlag = true;
	assert(Result->GenerateProfilingFlag == false && 
	       Result->GeneratePatternsFlag == false &&
	       "Only one mode can be selected.");
	break;
    }    
  } while (num > 1);
  
  if (Result->ProjectFile.size() == 0) {
    PrintUsage(argv[0], "Expecting project file name.\n");
    delete Result;
    return NULL;
  }
  
  Result->ArchName = ExtractArchName(Result->ProjectFile);
  
  if (! (Result->GenerateBackendFlag || Result->GenerateProfilingFlag 
      || Result->GeneratePatternsFlag)) {
    Result->GenerateBackendFlag = true;
    std::cout << "Generate compiler backend mode selected.\n";
  }
  
  // Extract information from environment variables 
  if (Result->GenerateBackendFlag) {
    char *LLVMDIRENV = getenv("LLVMDIR");
    if (!LLVMDIRENV) {
      std::cerr << "LLVMDIR environment variable must be set in order"
      " to start backend generation process.\n";
      delete Result;
      return NULL;
    }
    Result->LLVMDir = LLVMDIRENV;    
  }
  
  char *RULESFILEENV = getenv("RULESFILE");
  if (!RULESFILEENV) {
    std::cerr << "RULESFILE environment variable must be set"
      ".\n";
    delete Result;
    return NULL;
  }
  Result->RulesFile = RULESFILEENV;
  
  if (Result->GenerateBackendFlag) {
    char *TEMPLATEDIRENV = getenv("TEMPLATEDIR");
    if (!TEMPLATEDIRENV) {
      std::cerr << "TEMPLATEDIR environment variable must be set in order"      
	" to start backend generation process.\n";
      delete Result;
      return NULL;
    }
    Result->TemplateDir = TEMPLATEDIRENV;
  }
  
  return Result;
}


void CopyFile(const char *dst, const char *src) {
  std::ifstream ifs(src, std::ios::in | std::ios::binary);
  std::ofstream ofs(dst, std::ios::out | std::ios::binary | std::ios::trunc);

  ofs << ifs.rdbuf();
  ifs.close();
  ofs.close();
}

bool PatchLLVMConfigure(StartupInfo *SI, const string &ArchName) {
  // Now patch LLVM configure file 
  string ConfFN = SI->LLVMDir;
  ConfFN += "/configure";
  std::ifstream confs(ConfFN.c_str(), std::ios::in);
  string TmpFN = ConfFN;
  TmpFN += ".tmp";
  std::ofstream tmps(TmpFN.c_str(), std::ios::out);
  boost::regex e("^.*all\\)[ ]*TARGETS\\_TO\\_BUILD=\"((?:\\w+ ?)+)\".*$");
  boost::regex e2("cpp\\)[ ]*TARGETS\\_TO\\_BUILD=\"CppBackend \\$TARGETS\\_TO\\_BUILD\" ;;");
  string rep("  all) TARGETS_TO_BUILD=\"\\1 ");
  rep += ArchName;
  rep += "\" ;;";
  boost::smatch m;
  bool alreadyPatched = false;
  char buf[1024];

  while (confs.getline(buf, 1024)) {
    string curLine = buf;    
    if (boost::regex_search(curLine, m, e2)) {
      tmps << curLine << "\n";
      tmps << "        " << SI->ArchName << ")     TARGETS_TO_BUILD=\"" 
	   << ArchName << " $TARGETS_TO_BUILD\" ;;\n";
    } else if (boost::regex_search(curLine, m, e)) {
      //unsigned i;
      //for (i = 0; i < m.size(); ++i) {	
      //std::cout << m[i] << "|";
      //}
      string a = m[1];
      std::stringstream ss(a);
      while (ss >> a) {
	if (a == ArchName)
	  alreadyPatched = true;
      }
      if (alreadyPatched) {
	std::cout << "LLVM configure file is already patched. No need to modify it."
	  " Now you may build backend \"" << ArchName << "\" using LLVM build system.\n";
	return true;
      }
      // Needs patching      
      a = boost::regex_replace(curLine, e, rep, boost::match_default);
      tmps << a << "\n";
      //std::cout << "Teste\n";
    } else {
      tmps << curLine << "\n";
    }
  }

  confs.close();
  tmps.close();
  CopyFile(ConfFN.c_str(), TmpFN.c_str());
  std::cout << "LLVM configure file successfully patched. Now you may build backend \"" 
	    << ArchName << "\" using LLVM build system.\n";

  return true;

}

// This function will install a generated backend into LLVM source tree.
// Returns false if installation fails.
bool PatchLLVM(StartupInfo *SI, const char *SrcFilesDir) {
  struct stat sb;
  string BackendPath = SI->LLVMDir;
  BackendPath += "/lib/Target/";
  string ArchNameUcase = SI->ArchName;
  ArchNameUcase[0] = toupper(ArchNameUcase[0]);
  BackendPath += ArchNameUcase;
  
  // First, copy backend files
  if (stat(BackendPath.c_str(), &sb) == -1) {
    mkdir(BackendPath.c_str(), 0777);
  } else {
    std::cout << "Info: \"" << BackendPath << "\" already exists."
      " This operation overwrites all backend files.\n";
  }
  
  DIR *srcdir = opendir(SrcFilesDir);
  if (!srcdir) {
    std::cerr << "Unable to open llvmbackend source files directory.\n";
    return false;
  }
  
  struct dirent *de;
  while ((de = readdir(srcdir))) {
    string namesrc = SrcFilesDir;
    string namedst = BackendPath;
    namesrc += '/';
    namesrc += de->d_name;
    namedst += '/';
    namedst += de->d_name;
    CopyFile(namedst.c_str(), namesrc.c_str());    
  }    
  closedir(srcdir);
    
  return PatchLLVMConfigure(SI, ArchNameUcase);
}

void create_dir(const char *path) {
  struct stat sb;
  if (stat(path, &sb) == -1) {
    mkdir(path, 0777); 
  }
}

int main(int argc, char **argv) {
  unsigned Version;
  bool ForceCacheUsage = false;  
  std::cout << "\e[1;37mArchC LLVM Backend Generator (version 1.0).\e[0m\n";
  StartupInfo * SI = ProcessArguments(argc, argv);
  if (!SI) {    
    exit(EXIT_FAILURE);
  }    

  ForceCacheUsage = SI->ForceCacheFlag;

  std::cout << "Parsing input files...\n";
  
  helper::CMemWatcher *MemWatcher = helper::CMemWatcher::Instance();
  // FIXME: Temporary fix for myriad of leaks. DeallocateACParser()
  // should be used instead.
  MemWatcher->InstallHooks();  
  parse_archc_description(SI);
  Version = SaveAgent::CalculateVersion(SI->ISAFilename.c_str(),
	    SaveAgent::CalculateVersion(SI->RulesFile.c_str(),
	    SaveAgent::CalculateVersion(SI->BackendFile.c_str())));  
  MemWatcher->UninstallHooks();
  //print_formats();  
  //print_insns();    
  
  std::cout << "Building internal structures...\n";

  // Build information needed to parse backend generation file
  BuildFormats();
  BuildInsn(SI->VerboseFlag);  
  
  std::cout << "Parsing compiler info file...\n";
  if (!ParseBackendInformation(SI->RulesFile.c_str(), SI->BackendFile.c_str())) {
    DeallocateFormats();
    MemWatcher->ReportStatistics(std::cout);
    MemWatcher->FreeAll();
    helper::CMemWatcher::Destroy();
    exit(EXIT_FAILURE);
  }
  
  if (SI->GenerateBackendFlag || SI->GeneratePatternsFlag) {
    const char *TmpDir = "llvmbackend";
    create_dir(TmpDir);
    
    // Create the LLVM Instruction Formats file for target architecture
    string FormatsFile = "llvmbackend/";
    string ArchNameUcase = SI->ArchName;
    ArchNameUcase[0] = toupper(ArchNameUcase[0]);
    FormatsFile.append(ArchNameUcase);
    FormatsFile.append("InstrFormats.td");
    std::ofstream O;
    ArchEmitter AEmitter = ArchEmitter();
    O.open(FormatsFile.c_str(), std::ios::out | std::ios::trunc); 
    AEmitter.EmitInstrutionFormatClasses(FormatMap, BaseFormatNames, O, 
				       ArchNameUcase.c_str());
    O.close();

    // Create LLVM backend files based on template files
    TemplateManager TM(RuleManager, InstructionManager, RegisterManager,
		       OperandTable, OperatorTable, PatMan, Version,
		       ForceCacheUsage);
    TM.SetArchName(SI->ArchName.c_str());
    TM.SetCommentChar(ac_asm_get_comment_chars()[0]);
    TM.SetNumRegs(48);
    TM.SetWorkingDir(TmpDir);
    TM.SetTemplateDir(SI->TemplateDir.c_str());
    TM.SetIsBigEndian(ac_tgt_endian == 1? true: false);
    TM.SetWordSize(wordsize);
    TM.CreateBackendFiles();
  }
  
  if (SI->GenerateBackendFlag) {
    const char *TmpDir = "llvmbackend";
    std::cout << "Patching LLVM source tree...\n";
    if (!PatchLLVM(SI, TmpDir)) {
      std::cout << "LLVM source tree patch Failed.\n";
      exit(EXIT_FAILURE);
    }    
  }
  
  if (SI->GenerateProfilingFlag) {
    const char *TmpDir = "asmprof";
    create_dir(TmpDir);
    std::cout << "Generating assembly profiling files...\n";
    AsmProfileGen APG(RuleManager, InstructionManager, RegisterManager,
		      OperandTable, OperatorTable, PatMan);
    APG.SetWorkingDir(TmpDir);
    APG.SetCommentChar(ac_asm_get_comment_chars()[0]);
    APG.Generate();
  }

  DeallocateFormats();
  MemWatcher->ReportStatistics(std::cout);
  MemWatcher->FreeAll();
  helper::CMemWatcher::Destroy();
  //  DeallocateACParser();
  
  return 0;
}
