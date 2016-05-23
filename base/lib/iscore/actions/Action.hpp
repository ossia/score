#pragma once
#include <QString>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <memory>
#include <unordered_map>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
class IdentifiedObjectAbstract;
namespace iscore
{
struct DocumentContext;
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
        enum class EnablementContext {
            Application, // always enabled
            Document, // checked for enablement when document changes
            Selection, // checked for enablement when selection changes
            Focus // checked for enablement when focus changes
        };

        Action(
                QAction* act,
                StringKey<Action> key,
                StringKey<ActionGroup> k,
                EnablementContext ctx,
                const QKeySequence& defaultShortcut):
            m_impl{act},
            m_key{std::move(key)},
            m_groupKey{std::move(k)},
            m_ctx{ctx},
            m_default{defaultShortcut},
            m_current{defaultShortcut}
        {
            m_impl->setShortcut(m_current);
        }

        Action(
                QAction* act,
                const char* key,
                const char* group_key,
                EnablementContext ctx,
                const QKeySequence& defaultShortcut):
            m_impl{act},
            m_key{key},
            m_groupKey{group_key},
            m_ctx{ctx},
            m_default{defaultShortcut},
            m_current{defaultShortcut}
        {
            m_impl->setShortcut(m_current);
        }

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
        EnablementContext m_ctx;
        QKeySequence m_default;
        QKeySequence m_current;
};

struct ActionCondition
{
        ActionCondition(StringKey<ActionCondition> k):
            m_key{k}
        {

        }

        virtual ~ActionCondition();

        virtual bool operator()(MaybeDocument) = 0;
        virtual void action(MaybeDocument) { }

        StringKey<ActionCondition> key() const
        { return m_key; }

        // The actions that are impacted by this condition.
        std::vector<StringKey<Action>> actions;

    private:
        StringKey<ActionCondition> m_key;
};

/**
 * @brief The DocumentActionCondition struct
 *
 * Will be checked when the document changes
 */
struct DocumentActionCondition : public ActionCondition
{
};

/**
 * @brief The FocusActionCondition struct
 *
 * Will be checked when the focus changes
 */
struct FocusActionCondition : public ActionCondition
{
};

/**
 * @brief The SelectionActionCondition struct
 *
 * Will be checked when the selection changes
 */
struct SelectionActionCondition : public ActionCondition
{
};

/**
 * @brief The CustomActionCondition struct
 *
 * Will be checked when the changed signal is emitted
 */
struct CustomActionCondition :
        public QObject,
        public ActionCondition
{
        Q_OBJECT

    signals:
        void changed(bool);
};

struct ActionManager :
        public QObject
{
        void insert(Action val)
        {
            m_container.insert(
                        std::make_pair(
                            val.key(),
                            std::move(val)));
        }

        void insert(std::vector<Action> vals)
        {
            for(auto& val : vals)
            {
                insert(std::move(val));
            }
        }

        auto& get() const
        { return m_container; }

        void reset(Document* doc);

        void insert(std::unique_ptr<DocumentActionCondition> cond)
        {
            ISCORE_ASSERT(bool(cond));
            ISCORE_ASSERT(m_docConditions.find(cond->key()) == m_docConditions.end());

            m_docConditions.insert(std::make_pair(cond->key(), std::move(cond)));
        }
        void insert(std::unique_ptr<FocusActionCondition> cond)
        {
            ISCORE_ASSERT(bool(cond));
            ISCORE_ASSERT(m_focusConditions.find(cond->key()) == m_focusConditions.end());

            m_focusConditions.insert(std::make_pair(cond->key(), std::move(cond)));
        }
        void insert(std::unique_ptr<SelectionActionCondition> cond)
        {
            ISCORE_ASSERT(bool(cond));
            ISCORE_ASSERT(m_selectionConditions.find(cond->key()) == m_selectionConditions.end());

            m_selectionConditions.insert(std::make_pair(cond->key(), std::move(cond)));
        }
        void insert(std::unique_ptr<CustomActionCondition> cond)
        {
            ISCORE_ASSERT(bool(cond));
            ISCORE_ASSERT(m_customConditions.find(cond->key()) == m_customConditions.end());

            m_customConditions.insert(std::make_pair(cond->key(), std::move(cond)));
        }

    private:
        void documentChanged(MaybeDocument doc);
        void focusChanged(MaybeDocument doc);
        void selectionChanged(MaybeDocument doc);
        void resetCustomActions(MaybeDocument doc);
        std::unordered_map<StringKey<Action>, Action> m_container;

        // Conditions for the enablement of the actions
        std::unordered_map<StringKey<ActionCondition>, std::unique_ptr<DocumentActionCondition>> m_docConditions;
        std::unordered_map<StringKey<ActionCondition>, std::unique_ptr<FocusActionCondition>> m_focusConditions;
        std::unordered_map<StringKey<ActionCondition>, std::unique_ptr<SelectionActionCondition>> m_selectionConditions;
        std::unordered_map<StringKey<ActionCondition>, std::unique_ptr<CustomActionCondition>> m_customConditions;

        QMetaObject::Connection focusConnection;
        QMetaObject::Connection selectionConnection;
};

class Menu
{
    public:
        struct is_toplevel{};
        Menu(QMenu* menu,
             StringKey<Menu> m):
            m_impl{menu},
            m_key{std::move(m)}
        {

        }

        Menu(QMenu* menu,
             StringKey<Menu> m,
             is_toplevel,
             int column = std::numeric_limits<int>::max() - 1):
            m_impl{menu},
            m_key{std::move(m)},
            m_col{column},
            m_toplevel{true}
        {

        }

        StringKey<Menu> key() const
        { return m_key; }

        QMenu* menu() const
        { return m_impl; }

        int column() const
        { return m_col; }

        bool toplevel() const
        { return m_toplevel; }
    private:
        QMenu* m_impl{};
        StringKey<Menu> m_key;
        int m_col = std::numeric_limits<int>::max() - 1;
        bool m_toplevel{};
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

        QToolBar* toolbar() const
        { return m_impl; }

        StringKey<Toolbar> key() const
        { return m_key; }

        int row() const { return m_row; }
        int column() const { return m_col; }
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
    public:
        void insert(Menu val)
        {
            m_container.insert(
                        std::make_pair(
                            val.key(),
                            std::move(val)));
        }

        void insert(std::vector<Menu> vals)
        {
            for(auto& val : vals)
            {
                insert(std::move(val));
            }
        }

        auto& get() { return m_container; }
        auto& get() const { return m_container; }

    private:
        std::unordered_map<StringKey<Menu>, Menu> m_container;

};

struct ToolbarManager
{
    public:
        void insert(Toolbar val)
        {
            m_container.insert(
                        std::make_pair(
                            val.key(),
                            std::move(val)));
        }

        void insert(std::vector<Toolbar> vals)
        {
            for(auto& val : vals)
            {
                insert(std::move(val));
            }
        }

        auto& get() const
        { return m_container; }

    private:
        std::unordered_map<StringKey<Toolbar>, Toolbar> m_container;
};
/**
  * Setup goes this way :
  * First, plug-in's Toolbars and Menus are registered, empty but with a key
  * Then, plug-in's actions are registered and added into the menus & toolbars
  *
  *
  */
}
