#pragma once
#include <QString>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <unordered_map>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
class IdentifiedObjectAbstract;
namespace iscore
{
struct ActionGroup
{
    public:
        ActionGroup(
                QString prettyName,
                StringKey<ActionGroup> key):
            m_name{std::move(prettyName)},
            m_key{std::move(key)}
        {

        }

        QString prettyName() const
        {
            return m_name;
        }

        StringKey<ActionGroup> key() const
        { return m_key; }

    private:
        QString m_name;
        StringKey<ActionGroup> m_key;
};

class Action
{
    public:
        Action(QAction* act, StringKey<Action> key, StringKey<ActionGroup> k);

        StringKey<Action> key() const
        { return m_key; }

        QAction* action() const
        { return m_impl; }

        QKeySequence shortcut()
        {
            return m_current;
        }
        void setShortcut(const QKeySequence& shortcut)
        {
            m_current = shortcut;
            m_impl->setShortcut(shortcut);
        }

        QKeySequence defaultShortcut()
        {
            return m_default;
        }

    private:
        QAction* m_impl{};
        StringKey<Action> m_key;
        StringKey<ActionGroup> m_groupKey;
        QKeySequence m_default;
        QKeySequence m_current;
};


struct ActionManager
{
        void registerAction(Action action)
        {
            m_actions.insert(action);
        }

        auto& actions() const
        { return m_actions; }

    private:
        std::unordered_map<StringKey<Action>, Action> m_actions;

};

class Menu
{
        StringKey<Menu> key() const
        { return m_key; }

        QMenu* m_impl{};
        StringKey<Menu> m_key;
};


class Toolbar
{
    public:
        Toolbar(QToolBar* tb,
                StringKey<Toolbar> key,
                int defaultRow,
                int defaultCol):
            m_impl{tb},
            m_key{std::move(key)},
            m_defaultRow{defaultRow},
            m_defaultCol{defaultCol}
        {

        }

        StringKey<Toolbar> key() const
        { return m_key; }

    private:
        QToolBar* m_impl{};
        StringKey<Toolbar> m_key;

        // If a row is used, it goes next
        // Maybe it should be a list instead ?
        int m_row = 0;
        int m_col = 0;

        int m_defaultRow = 0;
        int m_defaultCol = 0;
};

struct MenuManager
{
        // Registers menus
        auto& actions() const
        { return m_actions; }

    private:
        std::unordered_map<StringKey<Action>, Action> m_actions;

};

struct ToolbarManager
{
        // Registers toolbars
};
/**
  * Setup goes this way :
  * First, plug-in's Toolbars and Menus are registered, empty but with a key
  * Then, plug-in's actions are registered and added into the menus & toolbars
  *
  *
  */
}
