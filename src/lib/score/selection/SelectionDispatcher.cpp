// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SelectionDispatcher.hpp"

#include "SelectionStack.hpp"

#include <score/selection/Selection.hpp>
namespace score
{
void SelectionDispatcher::setAndCommit(const Selection& s)
{
  m_stack.pushNewSelection(s);
}

score::SelectionStack& SelectionDispatcher::stack() const
{
  return m_stack;
}
}
