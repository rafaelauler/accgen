//===- Support.h - Header file for support classes ---------*- C++ -*------===//
//
//------------------------------------------------------------------------------
// Support classes and templates
//------------------------------------------------------------------------------

#include <list>
#include <climits>
#include <cstdlib>

namespace backendgen {

template<class T, class OP, class Function>
bool ApplyToLeafs(T root, Function f) {
  bool ReturnOk = true; 
  std::list<T> Queue;
  Queue.push_back(root);
  while (Queue.size() > 0) {
    T Element = Queue.front();
    Queue.pop_front();
    OP Op = dynamic_cast<OP>(Element);
    if (Op != NULL) {
      for (int I = 0, E = Op->getArity(); I != E; ++I) {
	Queue.push_back((*Op)[I]);
      }
      continue;
    }

    ReturnOk = ReturnOk && f(Element);
  }

  return ReturnOk;
}

template<class T, class OP, class Function>
bool ApplyToAllNodes(T root, Function f) {
  bool ReturnOk = true; 
  // No nodes to apply
  if (root == NULL)
    return false;
  std::list<T> Queue;
  Queue.push_back(root);
  while (Queue.size() > 0) {
    T Element = Queue.front();
    Queue.pop_front();
    if (Element == NULL)
      continue;
    OP Op = dynamic_cast<OP>(Element);
    if (Op != NULL) {
      ReturnOk = ReturnOk && f(Element);
      for (int I = 0, E = Op->getArity(); I != E; ++I) {
	Queue.push_back((*Op)[I]);
      }
      continue;
    }

    ReturnOk = ReturnOk && f(Element);
  }

  return ReturnOk;
}

inline unsigned ExtractOperandNumber(const std::string& OpName) {
  std::string::size_type idx;
  //std::cout << "ExtractOperandNumber called with " << OpName << "\n";
  idx = OpName.find_first_of("0123456789");    
  // "Operand name does not have a sequence number"
  if (idx == std::string::npos)
    return INT_MAX;
  return atoi(OpName.substr(idx).c_str());
}

// This auxiliary function checks if the operand has an operand number,
// which means it may be assigned by the assembler via assembly instruction
// syntax.
// TODO: Provide a more formal way of assigning operands to the 
// assembly writer, rather than "guessing" based on the presence of
// numbers in the operand's name.
inline bool HasOperandNumber(const std::string& OpName) {
  std::string::size_type idx;
  idx = OpName.find_first_of("0123456789");
  return (idx != std::string::npos);
}


}
