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
struct ISCORE_LIB_BASE_EXPORT ActionGroup
{
    public:
        ActionGroup(
                QString prettyName,
                StringKey<ActionGroup> key);

        QString prettyName() const;

        StringKey<ActionGroup> key() const;

    private:
        QString m_name;
        StringKey<ActionGroup> m_key;
};

class ISCORE_LIB_BASE_EXPORT Action
{
    public:
        Action(QAction* act,
               StringKey<Action> key,
               StringKey<ActionGroup> k,
               const QKeySequence& defaultShortcut);

        Action(QAction* act,
               const char* key,
               const char* group_key,
               const QKeySequence& defaultShortcut);

        StringKey<Action> key() const;

        QAction* action() const;

        QKeySequence shortcut();
        void setShortcut(const QKeySequence& shortcut);

        QKeySequence defaultShortcut();

    private:
        QAction* m_impl{};
        StringKey<Action> m_key;
        StringKey<ActionGroup> m_groupKey;
        QKeySequence m_default;
        QKeySequence m_current;
};

struct ISCORE_LIB_BASE_EXPORT ActionCondition
{
        ActionCondition(StringKey<ActionCondition> k);

        virtual ~ActionCondition();

        virtual void action(ActionManager& mgr, MaybeDocument);

        StringKey<ActionCondition> key() const;


        void setEnabled(iscore::ActionManager& mgr, bool b);

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
struct ISCORE_LIB_BASE_EXPORT DocumentActionCondition : public ActionCondition
{
        using ActionCondition::ActionCondition;
};

struct ISCORE_LIB_BASE_EXPORT EnableActionIfDocument final :
        public DocumentActionCondition
{
        EnableActionIfDocument():
            DocumentActionCondition{static_key()}
        {

        }

        static StringKey<ActionCondition> static_key()
        { return StringKey<ActionCondition>{"EnableActionIfDocument"}; }

        void action(ActionManager& mgr, MaybeDocument doc) override;
};

/**
 * @brief The FocusActionCondition struct
 *
 * Will be checked when the focus changes
 */
struct ISCORE_LIB_BASE_EXPORT FocusActionCondition : public ActionCondition
{
        using ActionCondition::ActionCondition;
};

/**
 * @brief The SelectionActionCondition struct
 *
 * Will be checked when the selection changes
 */
struct ISCORE_LIB_BASE_EXPORT SelectionActionCondition : public ActionCondition
{
        using ActionCondition::ActionCondition;
};

/**
 * @brief The CustomActionCondition struct
 *
 * Will be checked when the changed signal is emitted
 */
struct ISCORE_LIB_BASE_EXPORT CustomActionCondition :
        public QObject,
        public ActionCondition
{
        Q_OBJECT

    public:
        using ActionCondition::ActionCondition;

    signals:
        void changed(bool);
};

struct ISCORE_LIB_BASE_EXPORT ActionManager :
        public QObject
{
        ActionManager();

        void insert(Action val);

        void insert(std::vector<Action> vals);

        auto& get() const { return m_container; }

        void reset(Document* doc);

        void insert(std::unique_ptr<DocumentActionCondition> cond);
        void insert(std::unique_ptr<FocusActionCondition> cond);
        void insert(std::unique_ptr<SelectionActionCondition> cond);
        void insert(std::unique_ptr<CustomActionCondition> cond);

        const auto& documentConditions() const { return m_docConditions; }
        const auto& focusConditions() const { return m_focusConditions; }
        const auto& selectionConditions() const { return m_selectionConditions; }
        const auto& customConditions() const { return m_customConditions; }

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

class ISCORE_LIB_BASE_EXPORT Menu
{
    public:
        struct is_toplevel{};
        Menu(QMenu* menu,
             StringKey<Menu> m);

        Menu(QMenu* menu,
             StringKey<Menu> m,
             is_toplevel,
             int column = std::numeric_limits<int>::max() - 1);

        StringKey<Menu> key() const;

        QMenu* menu() const;

        int column() const;

        bool toplevel() const;
    private:
        QMenu* m_impl{};
        StringKey<Menu> m_key;
        int m_col = std::numeric_limits<int>::max() - 1;
        bool m_toplevel{};
};


class ISCORE_LIB_BASE_EXPORT Toolbar
{
    public:
        Toolbar(QToolBar* tb,
                StringKey<Toolbar> key,
                int defaultRow,
                int defaultCol);

        QToolBar* toolbar() const;

        StringKey<Toolbar> key() const;

        int row() const;
        int column() const;
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

struct ContextPoint
{
        QPointF scene;
        QPoint view;
};

struct ISCORE_LIB_BASE_EXPORT ContextMenuBuilder
{

    public:
        ContextMenuBuilder(StringKey<ContextMenuBuilder> k);

        virtual ~ContextMenuBuilder();

        // A condition may be : "selection.size() == 1 && selection.object() == statemodel"
        virtual bool check(const iscore::DocumentContext&, ContextPoint pts, QMenu*) const = 0;

        // In this case, we can add "play (states)" action.
        virtual void action(const iscore::DocumentContext&, ContextPoint pts, QMenu*) const = 0;

        StringKey<ContextMenuBuilder> key() const;

        // The actions that are impacted by this condition.
        std::vector<std::function<void(const iscore::DocumentContext&, ContextPoint, QMenu*)>> actions;

    private:
        StringKey<ContextMenuBuilder> m_key;
};
struct ISCORE_LIB_BASE_EXPORT MenuManager
{
    public:
        void insert(Menu val);

        void insert(std::vector<Menu> vals);

        void buildContextMenu(const iscore::DocumentContext& ctx, ContextPoint pts, QMenu* menu);

        auto& get()
        { return m_container; }
        auto& get() const
        { return m_container; }

    private:
        std::unordered_map<StringKey<Menu>, Menu> m_container;
        std::unordered_map<StringKey<ContextMenuBuilder>, std::unique_ptr<ContextMenuBuilder>> m_builders;
};

struct ISCORE_LIB_BASE_EXPORT ToolbarManager
{
    public:
        void insert(Toolbar val);

        void insert(std::vector<Toolbar> vals);

        auto& get() const
        { return m_container; }

    private:
        std::unordered_map<StringKey<Toolbar>, Toolbar> m_container;
};


using GUIElements = std::tuple<std::vector<Menu>, std::vector<Toolbar>, std::vector<Action>>;
using GUIElementsRef = std::tuple<std::vector<Menu>&, std::vector<Toolbar>&, std::vector<Action>&>;

}
