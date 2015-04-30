#include "SelectionStack.hpp"
using namespace iscore;

SelectionStack::SelectionStack()
{
    connect(this, &SelectionStack::pushNewSelection, this, &SelectionStack::push, Qt::QueuedConnection);
    m_unselectable.push({});
}

bool SelectionStack::canUnselect() const
{
    return m_unselectable.size() > 1;
}

bool SelectionStack::canReselect() const
{
    return !m_reselectable.empty();
}

void SelectionStack::push(const Selection& s)
{
    if(s.toSet() != m_unselectable.top().toSet())
    {
        for(QObject* obj : s)
        {
            connect(obj,  &QObject::destroyed,
                    this, &SelectionStack::prune);
        }
        m_unselectable.push(s);
        m_reselectable.clear();

        emit currentSelectionChanged(s);
    }
}

void SelectionStack::unselect()
{
    m_reselectable.push(m_unselectable.pop());

    emit currentSelectionChanged(m_unselectable.top());
}

void SelectionStack::reselect()
{
    m_unselectable.push(m_reselectable.pop());

    emit currentSelectionChanged(m_unselectable.top());
}

void SelectionStack::deselect()
{
    push({});
}

Selection SelectionStack::currentSelection() const
{
    return canUnselect()? m_unselectable.top() : Selection{};
}

void SelectionStack::prune(QObject* p)
{
    for(auto& sel : m_unselectable)
    {
        sel.removeAll(p);
    }

    for(auto& sel : m_reselectable)
    {
        sel.removeAll(p);
    }

    emit currentSelectionChanged(m_unselectable.top());
}
