//===- Support.h - Header file for support classes ---------*- C++ -*------===//
//
//------------------------------------------------------------------------------
// Support classes and templates
//------------------------------------------------------------------------------

#include <list>

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

}
