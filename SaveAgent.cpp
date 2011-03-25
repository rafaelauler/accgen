//                    -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode nil-*-
//===- SaveAgent.cpp - SearchResult Saver impl class ----------------------===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// These classes represent the agent responsible for saving discovered content
// about the machine, i.e., how to represent a particular fragment using 
// machine instructions. It is also responsible for loading back into memory
// all these contents.
//
//===----------------------------------------------------------------------===//


#include "SaveAgent.h"
#include "Support.h"
#include "InsnSelector/TransformationRules.h"
#include <string>
#include <limits>
#include <fstream>

using namespace backendgen;
using std::string;
using std::ofstream;
using std::ifstream;
using std::make_pair;

unsigned SaveAgent::CalculateVersion(const std::string &FileName,
				     unsigned chain) {
  ifstream File(FileName.c_str());
  unsigned hash = chain;
  char c = 0;
    
  while (File.get(c)) {
    unsigned hi;
    hash  = (hash << 4) + c;
    hi = hash & 0xf0000000;
    hash ^= hi;
    hash ^= hi >> 24;
  }
  return hash;
}

void SaveAgent::ClearFileAndSetVersion(unsigned Version) {
  ofstream File(SaveFile.c_str(), std::ios::out | std::ios::trunc);
    
  File << "VERSION: " << Version << "\n";
  
  if (!File)
    throw SaveException();
}

unsigned SaveAgent::CheckVersion() {
  ifstream File(SaveFile.c_str());
  string buf;
  unsigned version;
  
  File >> buf >> version;
  if (!File)
    return 0;
  if (buf.compare("VERSION:"))
    return 0;
  return version;
}

void SaveAgent::SaveRecord(SearchResult* SR, const string &Name) {
  ofstream File(SaveFile.c_str(), std::ios::out | std::ios::app);
  
  if (!File)
    throw SaveException();
  
  File << "PATTERN: " << Name << "\n";
  // Saving instruction list
  for (InstrList::const_iterator I = SR->Instructions->begin(),
    E = SR->Instructions->end(); I != E; ++I) {
    File << I->first->getLLVMName() << " " 
         << I->first->getIteratorPos(I->second) << " ";
  }
  File << "ENDOFSTREAM 0 \n";
  // Saving Cost
  File << SR->Cost << "\n";
  // Saving OperandsDefs
  for (OperandsDefsType::const_iterator I = SR->OperandsDefs->begin(),
    E = SR->OperandsDefs->end(); I != E; ++I) {
    for (NameListType::const_iterator I2 = (*I)->begin(), E2 = (*I)->end();
      I2 != E2; ++I2) {
      File << *I2 << " ";
    }
    File << "; ";
  }
  File << "ENDOFSTREAM \n";
  // Saving RulesApplied
  for (RulesAppliedList::const_iterator I = SR->RulesApplied->begin(),
    E = SR->RulesApplied->end(); I != E; ++I) {
    File << *I << " ";
  }
  File << "999999 \n";
  // Saving OpTrans
  for (OpTransLists::const_iterator I = SR->OpTrans->begin(), 
    E = SR->OpTrans->end(); I != E; ++I) {
    for (OperandTransformationList::const_iterator I2 = I->begin(),
    E2 = I->end(); I2 != E2; ++I2) {
      File << I2->LHSOperand << " " << I2->RHSOperand << " \"" 
           << I2->TransformExpression << "\" ";
    }
    File << "; ; ";
  }
  File << "ENDOFSTREAM A \n";
  for (VirtualToRealMap::const_iterator I = SR->ST->getVR()->begin(),
	   E = SR->ST->getVR()->end(); I != E; ++I) {
    File << I->first << " " << I->second << " ";
  }
  File << "ENDOFSTREAM A \n";
  
  if (!File)
    throw SaveException();
  return;
}

namespace {
bool findRecord(std::istream &I, const string &Name) {
  string buf;
  I.seekg(0, std::ios::beg);
  while (I >> buf) {    
    if (buf.compare("PATTERN:")) {
      I.ignore(std::numeric_limits<int>::max(), '\n');
      continue;
    }
    I >> buf;
    I.ignore(std::numeric_limits<int>::max(), '\n');
    if (!buf.compare(Name))
      return true;    
  }
  return false;
}
}


SearchResult* SaveAgent::LoadRecord(const string &Name) {
  ifstream File(SaveFile.c_str());  
  string buf1, buf2, buf3;
  unsigned buf4;
  
  if (!findRecord(File, Name))
    return NULL;
  
  SearchResult *SR = new SearchResult();
  
  //Load instruction list
  InstrList* Instructions = new InstrList();
  SR->Instructions = Instructions;
  while (File >> buf1 >> buf4) {
    if (!buf1.compare("ENDOFSTREAM"))
      break;
    const Instruction* ins = InstructionManager.getInstruction(buf1);
    if (ins == NULL) {
      delete SR;
      throw SaveException();
    }
    SemanticIterator SI;
    try {
      SI = ins->getIteratorAtPos(buf4);
    } catch (InvalidIteratorPosException) {
      AbortOn("Error: Cache file is corrupted. Delete cache.file or avoid"
	      " using \"-f\" flag.\n");
    } 
    Instructions->push_back(make_pair(ins, SI));
  }
  File.ignore(std::numeric_limits<int>::max(), '\n');
  
  //Load cost
  CostType Cost;
  File >> Cost;
  if (!File) {
    delete SR;
    throw SaveException();
  }
  File.ignore(std::numeric_limits<int>::max(), '\n');
  SR->Cost = Cost;
  
  //Loading OperandsDefs
  OperandsDefsType *OperandsDefs = new OperandsDefsType();
  SR->OperandsDefs = OperandsDefs;
  NameListType *NL = new NameListType();
  while (File >> buf1) {
    if (!buf1.compare(";")) {
      OperandsDefs->push_back(NL);
      NL = new NameListType();
      continue;
    } 
    if (!buf1.compare("ENDOFSTREAM")) {
      delete NL;
      break;
    }
    NL->push_back(buf1);
  }
  File.ignore(std::numeric_limits<int>::max(), '\n');
  
  if (!File) {
    delete SR;
    throw SaveException();
  }
  
  // Loading RulesApplied
  RulesAppliedList *RulesApplied = new RulesAppliedList();
  SR->RulesApplied = RulesApplied;
  while (File >> buf4) {
    if (buf4 == 999999)
      break;
    RulesApplied->push_back(buf4);
  }
  File.ignore(std::numeric_limits<int>::max(), '\n');
  
  // Loading OpTrans
  OpTransLists *OpTrans = new OpTransLists();
  SR->OpTrans = OpTrans;
  OperandTransformationList OTL = OperandTransformationList();
  while (File >> buf1 >> buf2) {
    if (!buf1.compare(";")) {
      OpTrans->push_back(OTL);
      OTL = OperandTransformationList();
      continue;
    }
    if (!buf1.compare("ENDOFSTREAM")) {
      break;
    }
    buf3.clear();    
    File.ignore(10, '\"');
    char c;
    File.get(c);
    while (c != '\"') {
      buf3 += c;
      File.get(c);
      if (!File) {
	delete SR;
	throw SaveException();
      }
    }
    OTL.push_back(OperandTransformation(buf1, buf2, buf3));
  }
  File.ignore(std::numeric_limits<int>::max(), '\n');
  
  // Loading VirtualToReal
  VirtualToRealMap *VirtualToReal = SR->ST->getVR();
  while (File >> buf1 >> buf2) {
    if (!buf1.compare("ENDOFSTREAM")) 
      break;
    VirtualToReal->push_back(make_pair(buf1, buf2));
  }
  
  if (!File) {
    delete SR;
    throw SaveException();
  }
  
  return SR;
}
