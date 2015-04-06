#include "SelectionDispatcher.hpp"
#include "SelectionStack.hpp"

using namespace iscore;

void SelectionDispatcher::set(const Selection& s)
{
    m_selection = s;
}

void SelectionDispatcher::append(const Selection::value_type& obj)
{
    if(!m_selection.contains(obj))
    {
        m_selection.append(obj);
        m_stack.push(m_selection);
    }
}

void SelectionDispatcher::remove(const Selection::value_type& obj)
{
    m_selection.removeAll(obj);
    m_stack.push(m_selection);
}

void SelectionDispatcher::setAndCommit(const Selection& s)
{
    m_stack.push(s);
    m_selection.clear();
}

void SelectionDispatcher::commit()
{
    // TODO this should merge the states corresponding
    // to the elements that were successfully clicked.

    m_stack.push(m_selection);
    m_selection.clear();
}

iscore::SelectionStack &SelectionDispatcher::stack() const
{
    return m_stack;
}
