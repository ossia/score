// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ToolbarManager.hpp"

#include <QToolBar>
namespace score
{

Toolbar::Toolbar(QToolBar* tb, StringKey<Toolbar> key, int defaultRow, int defaultCol)
    : m_impl{tb}
    , m_key{std::move(key)}
    , m_row{defaultRow}
    , m_col{defaultCol}
{
}

QToolBar* Toolbar::toolbar() const
{
  return m_impl;
}

StringKey<Toolbar> Toolbar::key() const
{
  return m_key;
}

int Toolbar::row() const
{
  return m_row;
}

int Toolbar::column() const
{
  return m_col;
}

ToolbarManager::~ToolbarManager()
{
  // Sole owner without a main window (MinimalApplication); with one, the
  // toolbars may already be gone with it, and the QPointer reads null.
  // Runs from ~Presenter, before the plug-ins that built the toolbars are
  // deleted: pointers they keep into a toolbar must be guarded.
  for(auto& tb : m_container)
  {
    if(QToolBar* t = tb.second.toolbar())
    {
      delete t;
    }
  }
  m_container.clear();
}

void ToolbarManager::insert(Toolbar val)
{
  m_container.insert(std::make_pair(val.key(), std::move(val)));
}

void ToolbarManager::insert(std::vector<Toolbar> vals)
{
  for(auto& val : vals)
  {
    insert(std::move(val));
  }
}
}
