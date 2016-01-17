#include <iscore/tools/IdentifiedObjectAbstract.hpp>
#include <QList>
#include <qnamespace.h>
#include <QPointer>
#include <QVector>
#include <algorithm>

#include "SelectionStack.hpp"
#include <iscore/selection/Selection.hpp>

namespace iscore
{
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

void SelectionStack::clear()
{
    m_unselectable.clear();
    m_reselectable.clear();
    m_unselectable.push(Selection{});
}

void SelectionStack::push(const Selection& selection)
{
    // TODO don't push "empty" selections, just add a "deselected" mode.
    if(selection != m_unselectable.top())
    {
        auto s = selection;
        auto it = s.begin();
        while(it != s.end())
        {
            if(*it) ++it;
            else it = s.erase(it);
        }
        for(const IdentifiedObjectAbstract* obj : s)
        {
            connect(obj,  &IdentifiedObjectAbstract::identified_object_destroyed,
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

void SelectionStack::prune(IdentifiedObjectAbstract* p)
{
    for(Selection& sel : m_unselectable)
    { // OPTIMIZEME should be removeOne
        sel.removeAll(p);

        for(auto it = sel.begin(); it != sel.end();)
        {
            // We prune the QPointer that might have been invalidated.
            // This is because if we remove multiple elements at the same time
            // some might still be in the list after the first destroyed() call; they will be refreshed and may lead to crashes.
            if((*it).isNull())
            {
                it = sel.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    for(Selection& sel : m_reselectable)
    {
        sel.removeAll(p);
        for(auto it = sel.begin(); it != sel.end();)
        {
            if((*it).isNull())
            {
                it = sel.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    m_unselectable.erase(std::remove_if(
                             m_unselectable.begin(),
                             m_unselectable.end(),
                             [] (const Selection& s ) { return s.empty(); }),
                         m_unselectable.end());

    m_reselectable.erase(
                std::remove_if(
                    m_reselectable.begin(),
                    m_reselectable.end(),
                    [] (const Selection& s) { return s.empty(); }),
                m_reselectable.end());

    if(m_unselectable.size() == 0)
        m_unselectable.push(Selection{});

    emit currentSelectionChanged(m_unselectable.top());
}
}
