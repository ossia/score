#include "Action.hpp"
#include <core/presenter/DocumentManager.hpp>
#include <iscore/actions/ActionManager.hpp>
#include <iscore/selection/SelectionStack.hpp>
namespace iscore
{

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
    , m_text{text}
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
    , m_text{text}
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
    , m_text{text}
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

Menu::Menu(QMenu* menu, StringKey<Menu> m) : m_impl{menu}, m_key{std::move(m)}
{
}

Menu::Menu(QMenu* menu, StringKey<Menu> m, Menu::is_toplevel, int column)
    : m_impl{menu}, m_key{std::move(m)}, m_col{column}, m_toplevel{true}
{
}

StringKey<Menu> Menu::key() const
{
  return m_key;
}

QMenu* Menu::menu() const
{
  return m_impl;
}

int Menu::column() const
{
  return m_col;
}

bool Menu::toplevel() const
{
  return m_toplevel;
}

Toolbar::Toolbar(
    QToolBar* tb, StringKey<Toolbar> key, int defaultRow, int defaultCol)
    : m_impl{tb}
    , m_key{std::move(key)}
    , m_row{defaultRow}
    , m_col{defaultCol}
    , m_defaultRow{defaultRow}
    , m_defaultCol{defaultCol}
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
}
