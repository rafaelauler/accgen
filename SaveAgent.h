//                    -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode nil-*-
//===- SaveAgent.h - Header file for SearchResult Saver impl class --------===//
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

#ifndef SAVEAGENT_H
#define SAVEAGENT_H

#include "InsnSelector/Search.h"
#include "InsnSelector/Semantic.h"

namespace backendgen {
  
  struct SaveException{};
  
  class SaveAgent {
    InstrManager& InstructionManager;
    std::string SaveFile;
    
    public:
      SaveAgent(InstrManager& I, const std::string &SaveFile):
        InstructionManager(I), SaveFile(SaveFile) {}
      
      static unsigned CalculateVersion(const std::string &FileName, 
				       unsigned chain = 0);
      void ClearFileAndSetVersion(unsigned Version);
      unsigned CheckVersion();
      void SaveRecord(SearchResult* SR, const std::string &Name);
      SearchResult* LoadRecord(const std::string &Name);
  };
  
  
}

#endif