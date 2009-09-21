
#ifndef ARCHEMITTER_H
#define ARCHEMITTER_H

#include <fstream>
#include <map>
#include <vector>

namespace backendgen {

class InsnFormat;
class FormatField;

class ArchEmitter {
public:
  ArchEmitter() {};
  ~ArchEmitter() {};
  virtual void EmitInstrutionFormatClasses
    (std::map<char *, InsnFormat*> FormatMap, std::vector<InsnFormat*> BaseFormatNames, std::ofstream &O);
  void EmitDeclareBitFieldMatch(FormatField &FF, std::ofstream &O);
  void EmitDeclareBitField(FormatField &FF, std::ofstream &O);
  void EmitDeclareClassArgs(InsnFormat &IF, std::ofstream &O, unsigned GrpNum);
  void EmitDeclareMatchAttr(InsnFormat &IF, std::ofstream &O, unsigned GrpNum);
  void EmitFormatClass(InsnFormat &IF, std::ofstream &O, unsigned GrpNumber);
};

};

#endif
