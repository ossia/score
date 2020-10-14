// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SelectionStack.hpp"

#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/selection/FocusManager.hpp>
#include <score/selection/Selectable.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/ForEach.hpp>

#include <ossia/detail/flat_set.hpp>

#include <QPointer>
#include <qnamespace.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::SelectionStack)
W_OBJECT_IMPL(Selectable)
W_OBJECT_IMPL(score::FocusManager)
void Selection::removeDuplicates()
{
  std::sort(begin(), end());
  std::unique(begin(), end());
}

Selectable::Selectable()
{
}

Selectable::~Selectable()
{
  set(false);
}

bool Selectable::get() const noexcept
{
  return m_val;
}

void Selectable::set(bool v)
{
  if (m_val != v)
  {
    m_val = v;
    changed(v);
  }
}

namespace score
{

SelectionStack::SelectionStack()
{
  connect(this, &SelectionStack::pushNewSelection, this, &SelectionStack::push);
  m_unselectable.push(Selection{});
}

SelectionStack::~SelectionStack()
{

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
  auto old = currentSelection();

  m_unselectable.clear();
  m_reselectable.clear();
  m_unselectable.push(Selection{});
  pruneConnections();

  currentSelectionChanged(old, m_unselectable.top());
}

void SelectionStack::clearAllButLast()
{
  Selection last;
  if (canUnselect())
    last = m_unselectable.top();

  m_unselectable.clear();
  m_reselectable.clear();
  m_unselectable.push(Selection{});
  m_unselectable.push(std::move(last));
  pruneConnections();
}

void SelectionStack::push(const Selection& selection)
{
  if (selection != m_unselectable.top())
  {
    auto old = currentSelection();
    auto s = selection;
    auto it = s.begin();
    while (it != s.end())
    {
      if (*it)
        ++it;
      else
        it = s.erase(it);
    }

    Foreach(s, [&](auto obj) {
      if (m_connections.find(obj) == m_connections.end())
      {
        QMetaObject::Connection con = connect(
            obj,
            &IdentifiedObjectAbstract::identified_object_destroyed,
            this,
            &SelectionStack::prune,
            Qt::UniqueConnection);
        m_connections.insert({obj, con});
      }
    });

    m_unselectable.push(s);

    if (m_unselectable.size() > 50)
    {
      m_unselectable.removeFirst();
    }
    m_reselectable.clear();

    pruneConnections();
    currentSelectionChanged(old, s);
  }
}

void SelectionStack::unselect()
{
  auto old = currentSelection();
  m_reselectable.push(m_unselectable.pop());

  if (m_unselectable.empty())
    m_unselectable.push(Selection{});

  currentSelectionChanged(old, m_unselectable.top());
}

void SelectionStack::reselect()
{
  auto old = currentSelection();
  m_unselectable.push(m_reselectable.pop());

  currentSelectionChanged(old, m_unselectable.top());
}

void SelectionStack::deselect()
{
  push(Selection{});
}

void SelectionStack::deselectObjects(const Selection& toDeselect)
{
  Selection s = currentSelection();
  for (auto& obj : toDeselect)
  {
    s.removeAll(obj);
  }
  pushNewSelection(std::move(s));
}

Selection SelectionStack::currentSelection() const
{
  return canUnselect() ? m_unselectable.top() : Selection{};
}

void SelectionStack::prune(IdentifiedObjectAbstract* p)
{
  {
    int n = m_unselectable.size();
    for (int i = 0; i < n; i++)
    {
      Selection& sel = m_unselectable[i];
      // OPTIMIZEME should be removeOne
      sel.removeAll(p);

      for (auto it = sel.begin(); it != sel.end();)
      {
        // We prune the QPointer that might have been invalidated.
        // This is because if we remove multiple elements at the same time
        // some might still be in the list after the first destroyed() call;
        // they will be refreshed and may lead to crashes.
        if ((*it).isNull())
        {
          it = sel.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }
  }

  {
    int n = m_reselectable.size();
    for (int i = 0; i < n; i++)
    {
      Selection& sel = m_reselectable[i];
      sel.removeAll(p);
      for (auto it = sel.begin(); it != sel.end();)
      {
        if ((*it).isNull())
        {
          it = sel.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }
  }

  m_unselectable.erase(
      std::remove_if(
          m_unselectable.begin(),
          m_unselectable.end(),
          [](const Selection& s) { return s.empty(); }),
      m_unselectable.end());

  m_reselectable.erase(
      std::remove_if(
          m_reselectable.begin(),
          m_reselectable.end(),
          [](const Selection& s) { return s.empty(); }),
      m_reselectable.end());

  if (m_unselectable.size() == 0)
    m_unselectable.push(Selection{});

  pruneConnections();
  currentSelectionChanged(m_unselectable.top(), m_unselectable.top());
}

void SelectionStack::pruneConnections()
{
  ossia::flat_set<const IdentifiedObjectAbstract*> present;
  for (auto& sel : m_unselectable)
  {
    for (auto& obj : sel)
    {
      present.insert(obj.data());
    }
  }
  for (auto& sel : m_reselectable)
  {
    for (auto& obj : sel)
    {
      present.insert(obj.data());
    }
  }

  std::vector<const IdentifiedObjectAbstract*> to_remove;
  for (auto& e : m_connections)
  {
    if (present.find(e.first) == present.end())
      to_remove.push_back(e.first);
  }

  for (auto ptr : to_remove)
  {
    auto it = m_connections.find(ptr);
    QObject::disconnect(it->second);
    m_connections.erase(it);
  }
}
}
