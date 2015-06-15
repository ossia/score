#include "SelectionStack.hpp"
using namespace iscore;

SelectionStack::SelectionStack()
{
    connect(this, &SelectionStack::pushNewSelection, this, &SelectionStack::push, Qt::QueuedConnection);
    m_unselectable.push(Selection{});
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
    if(s != m_unselectable.top())
    {
        for(const QObject* obj : s)
        {
            connect(obj,  &QObject::destroyed,
                    this, &SelectionStack::prune);
        }
        m_unselectable.push(s);

        if(m_unselectable.size() > 50)
        {
            m_unselectable.removeFirst();
        }
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
    push(Selection{});
}

Selection SelectionStack::currentSelection() const
{
    return canUnselect()? m_unselectable.top() : Selection{};
}

void SelectionStack::prune(QObject* p)
{
    for(auto& sel : m_unselectable)
    {
        sel.erase(p);
    }

    for(auto& sel : m_reselectable)
    {
        sel.erase(p);
    }

    std::remove_if(
                m_unselectable.begin(),
                m_unselectable.end(),
                [] (const Selection& s )
    { return s.empty(); });

    std::remove_if(
                m_reselectable.begin(),
                m_reselectable.end(),
                [] (const Selection& s )
    { return s.empty(); });

    if(m_unselectable.size() == 0)
        m_unselectable.push(Selection{});

    emit currentSelectionChanged(m_unselectable.top());
}
