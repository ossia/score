// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SelectionDispatcher.hpp"

#include "SelectionStack.hpp"

#include <score/selection/Selection.hpp>
#include <QApplication>
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
  auto ptr = const_cast<IdentifiedObjectAbstract*>(&s);
  if(qApp->keyboardModifiers() & Qt::CTRL)
  {
    auto sel = m_stack.currentSelection();
    if(!sel.contains(ptr))
      sel.append(ptr);

    m_stack.pushNewSelection(std::move(sel));
  }
  else
  {
    m_stack.pushNewSelection(
        Selection{const_cast<IdentifiedObjectAbstract*>(&s)});
  }
}

score::SelectionStack& SelectionDispatcher::stack() const
{
  return m_stack;
}
}
