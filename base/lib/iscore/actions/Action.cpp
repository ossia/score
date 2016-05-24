#include "Action.hpp"
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <iscore/selection/SelectionStack.hpp>
namespace iscore
{

ActionManager::ActionManager()
{
    insert(std::make_unique<EnableActionIfDocument>());

}

void ActionManager::insert(Action val)
{
    m_container.insert(
                std::make_pair(
                    val.key(),
                    std::move(val)));
}

void ActionManager::insert(std::vector<Action> vals)
{
    for(auto& val : vals)
    {
        insert(std::move(val));
    }
}

void ActionManager::reset(iscore::Document* doc)
{

    // Cleanup
    QObject::disconnect(focusConnection);
    QObject::disconnect(selectionConnection);

    MaybeDocument mdoc;
    if(doc)
    {
        mdoc = doc->context();
    }


    // Setup connections
    if(doc)
    {
        focusConnection =
                con(doc->focusManager(), &FocusManager::changed,
                    this, [=] { focusChanged(mdoc); });
        selectionConnection =
                con(doc->selectionStack(), &SelectionStack::currentSelectionChanged,
                    this, [=] (const auto&) { this->selectionChanged(mdoc); });
    }


    // Reset all the actions
    documentChanged(mdoc);
    focusChanged(mdoc);
    selectionChanged(mdoc);
}

void ActionManager::insert(std::unique_ptr<DocumentActionCondition> cond)
{
    ISCORE_ASSERT(bool(cond));
    ISCORE_ASSERT(m_docConditions.find(cond->key()) == m_docConditions.end());

    m_docConditions.insert(std::make_pair(cond->key(), std::move(cond)));
}

void ActionManager::insert(std::unique_ptr<FocusActionCondition> cond)
{
    ISCORE_ASSERT(bool(cond));
    ISCORE_ASSERT(m_focusConditions.find(cond->key()) == m_focusConditions.end());

    m_focusConditions.insert(std::make_pair(cond->key(), std::move(cond)));
}

void ActionManager::insert(std::unique_ptr<SelectionActionCondition> cond)
{
    ISCORE_ASSERT(bool(cond));
    ISCORE_ASSERT(m_selectionConditions.find(cond->key()) == m_selectionConditions.end());

    m_selectionConditions.insert(std::make_pair(cond->key(), std::move(cond)));
}

void ActionManager::insert(std::unique_ptr<CustomActionCondition> cond)
{
    ISCORE_ASSERT(bool(cond));
    ISCORE_ASSERT(m_customConditions.find(cond->key()) == m_customConditions.end());

    m_customConditions.insert(std::make_pair(cond->key(), std::move(cond)));
}

void ActionManager::documentChanged(MaybeDocument doc)
{
    for(auto& c_pair : m_docConditions)
    {
        DocumentActionCondition& cond = *c_pair.second;
        cond.action(*this, doc);
    }
}

void ActionManager::focusChanged(MaybeDocument doc)
{
    for(auto& c_pair : m_focusConditions)
    {
        FocusActionCondition& cond = *c_pair.second;
        cond.action(*this, doc);
    }
}

void ActionManager::selectionChanged(MaybeDocument doc)
{
    for(auto& c_pair : m_selectionConditions)
    {
        SelectionActionCondition& cond = *c_pair.second;
        cond.action(*this, doc);
    }
}

void ActionManager::resetCustomActions(MaybeDocument doc)
{
    for(auto& c_pair : m_customConditions)
    {
        CustomActionCondition& cond = *c_pair.second;
        cond.action(*this, doc);
    }
}

void EnableActionIfDocument::action(ActionManager& mgr, MaybeDocument doc)
{
    setEnabled(mgr, bool(doc));
}

ActionCondition::ActionCondition(StringKey<ActionCondition> k):
    m_key{std::move(k)}
{

}

ActionCondition::~ActionCondition()
{

}

void ActionCondition::action(ActionManager &mgr, MaybeDocument) { }

StringKey<ActionCondition> ActionCondition::key() const
{ return m_key; }

void ActionCondition::setEnabled(ActionManager &mgr, bool b)
{
    for(auto& action : actions)
    {
        auto& act = mgr.get().at(action);
        qDebug() << act.action()->text() << b;
        act.action()->setEnabled(b);
    }
}

ActionGroup::ActionGroup(QString prettyName, ActionGroupKey key):
    m_name{std::move(prettyName)},
    m_key{std::move(key)}
{

}

QString ActionGroup::prettyName() const
{
    return m_name;
}

ActionGroupKey ActionGroup::key() const
{ return m_key; }

Action::Action(QAction *act, ActionKey key, ActionGroupKey k, const QKeySequence &defaultShortcut):
    m_impl{act},
    m_key{std::move(key)},
    m_groupKey{std::move(k)},
    m_default{defaultShortcut},
    m_current{defaultShortcut}
{
    m_impl->setShortcut(m_current);
}

Action::Action(QAction *act, const char *key, const char *group_key, const QKeySequence &defaultShortcut):
    m_impl{act},
    m_key{key},
    m_groupKey{group_key},
    m_default{defaultShortcut},
    m_current{defaultShortcut}
{
    m_impl->setShortcut(m_current);
}

ActionKey Action::key() const
{ return m_key; }

QAction *Action::action() const
{ return m_impl; }

QKeySequence Action::shortcut()
{
    return m_current;
}

void Action::setShortcut(const QKeySequence &shortcut)
{
    m_current = shortcut;
    m_impl->setShortcut(shortcut);
}

QKeySequence Action::defaultShortcut()
{
    return m_default;
}

Menu::Menu(QMenu *menu, StringKey<Menu> m):
    m_impl{menu},
    m_key{std::move(m)}
{

}

Menu::Menu(QMenu *menu, StringKey<Menu> m, Menu::is_toplevel, int column):
    m_impl{menu},
    m_key{std::move(m)},
    m_col{column},
    m_toplevel{true}
{

}

StringKey<Menu> Menu::key() const
{ return m_key; }

QMenu *Menu::menu() const
{ return m_impl; }

int Menu::column() const
{ return m_col; }

bool Menu::toplevel() const
{ return m_toplevel; }

Toolbar::Toolbar(QToolBar *tb, StringKey<Toolbar> key, int defaultRow, int defaultCol):
    m_impl{tb},
    m_key{std::move(key)},
    m_row{defaultRow},
    m_col{defaultCol},
    m_defaultRow{defaultRow},
    m_defaultCol{defaultCol}
{

}

QToolBar *Toolbar::toolbar() const
{ return m_impl; }

StringKey<Toolbar> Toolbar::key() const
{ return m_key; }

int Toolbar::row() const { return m_row; }

int Toolbar::column() const { return m_col; }

ContextMenuBuilder::ContextMenuBuilder(StringKey<ContextMenuBuilder> k):
    m_key{std::move(k)}
{

}

ContextMenuBuilder::~ContextMenuBuilder()
{

}

StringKey<ContextMenuBuilder> ContextMenuBuilder::key() const
{ return m_key; }

void MenuManager::insert(Menu val)
{
    m_container.insert(
                std::make_pair(
                    val.key(),
                    std::move(val)));
}

void MenuManager::insert(std::vector<Menu> vals)
{
    for(auto& val : vals)
    {
        insert(std::move(val));
    }
}

void MenuManager::buildContextMenu(const DocumentContext &ctx, ContextPoint pts, QMenu *menu)
{
    for(auto& act : m_builders)
    {
        const ContextMenuBuilder& cmb = *act.second;
        if(cmb.check(ctx, pts, menu))
        {
            cmb.action(ctx, pts, menu);
        }
    }
}

void ToolbarManager::insert(Toolbar val)
{
    m_container.insert(
                std::make_pair(
                    val.key(),
                    std::move(val)));
}

void ToolbarManager::insert(std::vector<Toolbar> vals)
{
    for(auto& val : vals)
    {
        insert(std::move(val));
    }
}

}
