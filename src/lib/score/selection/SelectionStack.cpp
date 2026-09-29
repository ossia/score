// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SelectionStack.hpp"

#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/selection/FocusManager.hpp>
#include <score/selection/Selectable.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/ForEach.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/ssize.hpp>

#include <QPointer>
#include <qnamespace.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::SelectionStack)
W_OBJECT_IMPL(Selectable)
W_OBJECT_IMPL(score::FocusManager)
void Selection::removeDuplicates()
{
  ossia::remove_duplicates(*this);
}

Selectable::Selectable(QObject* parent)
    : QObject{parent}
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
  if(m_val != v)
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

SelectionStack::~SelectionStack() { }

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
  sweepPruned();
  auto old = currentSelection();

  m_unselectable.clear();
  m_reselectable.clear();
  m_unselectable.push(Selection{});
  pruneConnections();

  currentSelectionChanged(old, m_unselectable.top());
}

void SelectionStack::clearAllButLast()
{
  sweepPruned();
  Selection last;
  if(canUnselect())
    last = m_unselectable.top();

  m_unselectable.clear();
  m_reselectable.clear();
  m_unselectable.push(Selection{});
  m_unselectable.push(std::move(last));
  pruneConnections();
}

void SelectionStack::push(const Selection& selection)
{
  sweepPruned();
  if(selection != m_unselectable.top())
  {
    auto old = currentSelection();
    auto s = selection;
    auto it = s.begin();
    while(it != s.end())
    {
      if(*it)
        ++it;
      else
        it = s.erase(it);
    }

    Foreach(s, [&](const QPointer<IdentifiedObjectAbstract>& obj) {
      if(m_connections.find(obj.data()) == m_connections.end())
      {
        QMetaObject::Connection con = connect(
            obj, &IdentifiedObjectAbstract::identified_object_destroyed, this,
            &SelectionStack::prune, Qt::UniqueConnection);
        m_connections.insert({obj.data(), con});
      }
    });

    m_unselectable.push(s);

    // The oldest selection goes; the empty one at the bottom stays.
    if(m_unselectable.size() > 50)
    {
      m_unselectable.removeAt(1);
    }
    m_reselectable.clear();

    pruneConnections();
    currentSelectionChanged(old, s);
  }
}

void SelectionStack::unselect()
{
  sweepPruned();
  auto old = currentSelection();
  m_reselectable.push(m_unselectable.pop());

  if(m_unselectable.empty())
    m_unselectable.push(Selection{});

  currentSelectionChanged(old, m_unselectable.top());
}

void SelectionStack::reselect()
{
  sweepPruned();
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
  for(auto& obj : toDeselect)
  {
    s.removeAll(obj);
  }
  pushNewSelection(std::move(s));
}

static bool isPruned(
    const QPointer<IdentifiedObjectAbstract>& obj,
    const ossia::hash_set<const IdentifiedObjectAbstract*>& pruned)
{
  return obj.isNull() || pruned.contains(obj.data());
}

static void removePruned(
    Selection& sel, const ossia::hash_set<const IdentifiedObjectAbstract*>& pruned)
{
  sel.erase(
      std::remove_if(
          sel.begin(), sel.end(), [&](const auto& obj) { return isPruned(obj, pruned); }),
      sel.end());
}

Selection SelectionStack::currentSelection() const
{
  if(m_pruned.empty())
    return canUnselect() ? m_unselectable.top() : Selection{};

  // What the sweep will leave current: the topmost selection that keeps an
  // object, the empty selections being dropped.
  for(auto i = m_unselectable.size() - 1; i > 0; --i)
  {
    Selection s = m_unselectable[i];
    removePruned(s, m_pruned);
    if(!s.empty())
      return s;
  }
  return Selection{};
}

static void
dropEmptySelections(QStack<Selection>& unselectable, QStack<Selection>& reselectable)
{
  // The first one is the empty selection that stays at the bottom:
  // canUnselect() and currentSelection() count on it.
  if(!unselectable.empty())
    unselectable.erase(
        std::remove_if(
            unselectable.begin() + 1, unselectable.end(),
            [](const Selection& s) { return s.empty(); }),
        unselectable.end());

  reselectable.erase(
      std::remove_if(
          reselectable.begin(), reselectable.end(),
          [](const Selection& s) { return s.empty(); }),
      reselectable.end());

  if(unselectable.empty() || !unselectable.front().empty())
    unselectable.push_front(Selection{});
}

void SelectionStack::prune(IdentifiedObjectAbstract* p)
{
  if(m_batchDepth > 0)
  {
    m_pruned.insert(p);
    return;
  }

  {
    int n = std::ssize(m_unselectable);
    for(int i = 0; i < n; i++)
    {
      Selection& sel = m_unselectable[i];
      // OPTIMIZEME should be removeOne
      sel.removeAll(p);

      for(auto it = sel.begin(); it != sel.end();)
      {
        // We prune the QPointer that might have been invalidated.
        // This is because if we remove multiple elements at the same time
        // some might still be in the list after the first destroyed() call;
        // they will be refreshed and may lead to crashes.
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
  }

  {
    int n = std::ssize(m_reselectable);
    for(int i = 0; i < n; i++)
    {
      Selection& sel = m_reselectable[i];
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
  }

  dropEmptySelections(m_unselectable, m_reselectable);
  pruneConnections();
  currentSelectionChanged(m_unselectable.top(), m_unselectable.top());
}

static std::vector<IdentifiedObjectAbstract*>
recursiveChildrenList(IdentifiedObjectAbstract* obj)
{
  std::vector<IdentifiedObjectAbstract*> vec;
  vec.reserve(4 * obj->children().size());
  vec.push_back(obj);

  std::function<void(IdentifiedObjectAbstract*)> rec;
  rec = [&](IdentifiedObjectAbstract* parent) {
    const auto& children = parent->children();
    for(auto it = children.cbegin(), end = children.cend(); it != end; ++it)
    {
      if(auto p = qobject_cast<IdentifiedObjectAbstract*>(*it))
      {
        vec.push_back(p);
        rec(p);
      }
    }
  };
  rec(obj);

  return vec;
}

void SelectionStack::sweepPruned()
{
  if(m_pruned.empty())
    return;

  for(auto& sel : m_unselectable)
    removePruned(sel, m_pruned);
  for(auto& sel : m_reselectable)
    removePruned(sel, m_pruned);
  m_pruned.clear();

  dropEmptySelections(m_unselectable, m_reselectable);
  pruneConnections();
  currentSelectionChanged(m_unselectable.top(), m_unselectable.top());
}

void SelectionStack::pruneRecursively(IdentifiedObjectAbstract* p)
{
  clearAllButLast();

  auto children = recursiveChildrenList(p);
  {
    int n = std::ssize(m_unselectable);
    for(int i = 0; i < n; i++)
    {
      Selection& sel = m_unselectable[i];
      // OPTIMIZEME should be removeOne

      for(IdentifiedObjectAbstract* obj : children)
        sel.removeAll(obj);

      for(auto it = sel.begin(); it != sel.end();)
      {
        // We prune the QPointer that might have been invalidated.
        // This is because if we remove multiple elements at the same time
        // some might still be in the list after the first destroyed() call;
        // they will be refreshed and may lead to crashes.
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
  }

  {
    int n = std::ssize(m_reselectable);
    for(int i = 0; i < n; i++)
    {
      Selection& sel = m_reselectable[i];

      for(IdentifiedObjectAbstract* obj : children)
        sel.removeAll(obj);

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
  }

  dropEmptySelections(m_unselectable, m_reselectable);
  pruneConnections();
  currentSelectionChanged(m_unselectable.top(), m_unselectable.top());
}

void SelectionStack::pruneConnections()
{
  ossia::flat_set<const IdentifiedObjectAbstract*> present;
  for(auto& sel : m_unselectable)
  {
    for(auto& obj : sel)
    {
      present.insert(obj.data());
    }
  }
  for(auto& sel : m_reselectable)
  {
    for(auto& obj : sel)
    {
      present.insert(obj.data());
    }
  }

  std::vector<const IdentifiedObjectAbstract*> to_remove;
  for(auto& e : m_connections)
  {
    if(present.find(e.first) == present.end())
      to_remove.push_back(e.first);
  }

  for(auto ptr : to_remove)
  {
    auto it = m_connections.find(ptr);
    QObject::disconnect(it->second);
    m_connections.erase(it);
  }
}
}
