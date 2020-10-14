// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SelectionDispatcher.hpp"

#include "SelectionStack.hpp"

#include <score/selection/Selection.hpp>
namespace score
{
static const Selection emptySelection;
void SelectionDispatcher::deselect()
{
  m_stack.pushNewSelection(emptySelection);
}

void SelectionDispatcher::select(const Selection& s)
{
  m_stack.pushNewSelection(s);
}

void SelectionDispatcher::select(const IdentifiedObjectAbstract& s)
{
  m_stack.pushNewSelection(Selection{const_cast<IdentifiedObjectAbstract*>(&s)});
}

score::SelectionStack& SelectionDispatcher::stack() const
{
  return m_stack;
}
}
