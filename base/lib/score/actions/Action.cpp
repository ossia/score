// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Action.hpp"

#include <core/presenter/DocumentManager.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/selection/SelectionStack.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(score::CustomActionCondition)
namespace score
{

EnableActionIfDocument::~EnableActionIfDocument()
{
}

void EnableActionIfDocument::action(ActionManager& mgr, MaybeDocument doc)
{
  setEnabled(mgr, bool(doc));
}

ActionCondition::ActionCondition(StringKey<ActionCondition> k)
    : m_key{std::move(k)}
{
}

ActionCondition::~ActionCondition()
{
}

void ActionCondition::action(ActionManager& mgr, MaybeDocument)
{
}

StringKey<ActionCondition> ActionCondition::key() const
{
  return m_key;
}

void ActionCondition::setEnabled(ActionManager& mgr, bool b)
{
  for (auto& action : m_actions)
  {
    auto& act = mgr.get().at(action);
    act.action()->setEnabled(b);
  }
}

ActionGroup::ActionGroup(QString prettyName, ActionGroupKey key)
    : m_name{std::move(prettyName)}, m_key{std::move(key)}
{
}

QString ActionGroup::prettyName() const
{
  return m_name;
}

ActionGroupKey ActionGroup::key() const
{
  return m_key;
}

Action::Action(
    QAction* act,
    QString text,
    ActionKey key,
    ActionGroupKey k,
    const QKeySequence& defaultShortcut)
    : m_impl{act}
    , m_text{std::move(text)}
    , m_key{std::move(key)}
    , m_groupKey{std::move(k)}
    , m_default{defaultShortcut}
    , m_current{defaultShortcut}
{
  m_impl->setShortcut(m_current);
  updateTexts();
}
Action::Action(
    QAction* act,
    QString text,
    ActionKey key,
    ActionGroupKey k,
    const QKeySequence& defaultShortcut,
    const QKeySequence& defaultShortcut2)
    : m_impl{act}
    , m_text{std::move(text)}
    , m_key{std::move(key)}
    , m_groupKey{std::move(k)}
    , m_default{defaultShortcut}
    , m_current{defaultShortcut}
{
  m_impl->setShortcuts({m_current, defaultShortcut2});
  updateTexts();
}

Action::Action(
    QAction* act,
    QString text,
    const char* key,
    const char* group_key,
    const QKeySequence& defaultShortcut)
    : m_impl{act}
    , m_text{std::move(text)}
    , m_key{key}
    , m_groupKey{group_key}
    , m_default{defaultShortcut}
    , m_current{defaultShortcut}
{
  m_impl->setShortcut(m_current);
  updateTexts();
}

ActionKey Action::key() const
{
  return m_key;
}

QAction* Action::action() const
{
  return m_impl;
}

QKeySequence Action::shortcut()
{
  return m_current;
}

void Action::setShortcut(const QKeySequence& shortcut)
{
  m_current = shortcut;
  m_impl->setShortcut(shortcut);
  updateTexts();
}

QKeySequence Action::defaultShortcut()
{
  return m_default;
}

void Action::updateTexts()
{
  m_impl->setText(m_text);

  QString clearText = m_text;
  clearText.remove('&');
  clearText.append(QString(" (%1)").arg(m_impl->shortcut().toString()));
  m_impl->setToolTip(clearText);
  m_impl->setWhatsThis(clearText);
  m_impl->setStatusTip(clearText);
}

DocumentActionCondition::~DocumentActionCondition()
{
}

FocusActionCondition::~FocusActionCondition()
{
}

SelectionActionCondition::~SelectionActionCondition()
{
}

CustomActionCondition::~CustomActionCondition()
{
}
}
