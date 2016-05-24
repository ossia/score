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
class ActionGroup;
using ActionGroupKey = StringKey<ActionGroup>;
class ISCORE_LIB_BASE_EXPORT ActionGroup
{
    public:
        ActionGroup(
                QString prettyName,
                ActionGroupKey key);

        QString prettyName() const;

        ActionGroupKey key() const;

    private:
        QString m_name;
        ActionGroupKey m_key;
};
class Action;
using ActionKey = StringKey<Action>;

class ISCORE_LIB_BASE_EXPORT Action
{
    public:
        Action(QAction* act,
               ActionKey key,
               ActionGroupKey k,
               const QKeySequence& defaultShortcut);

        Action(QAction* act,
               const char* key,
               const char* group_key,
               const QKeySequence& defaultShortcut);

        ActionKey key() const;

        QAction* action() const;

        QKeySequence shortcut();
        void setShortcut(const QKeySequence& shortcut);

        QKeySequence defaultShortcut();

    private:
        QAction* m_impl{};
        ActionKey m_key;
        ActionGroupKey m_groupKey;
        QKeySequence m_default;
        QKeySequence m_current;
};


template<typename Action_T>
struct MetaAction
{
        // static iscore::Action make(QAction* ptr);
        // static ActionKey key();
};

struct ActionContainer
{
    public:
        std::vector<Action> container;

        template<typename Action_T>
        void add(QAction* ptr)
        {
            ISCORE_ASSERT(find_if(container, [] (auto ac) { return ac.key() == MetaAction<Action_T>::key(); }) == container.end());
            container.emplace_back(MetaAction<Action_T>::make(ptr));
        }
};

struct ActionKeyContainer
{
    public:
        std::vector<ActionKey> actions;

        template<typename Action_T>
        void add()
        {
            ISCORE_ASSERT(find_if(actions, [] (auto ac) { return ac == MetaAction<Action_T>::key(); }) == actions.end());
            actions.emplace_back(MetaAction<Action_T>::key());
        }
};


struct ISCORE_LIB_BASE_EXPORT ActionCondition :
        public ActionKeyContainer
{
        ActionCondition(StringKey<ActionCondition> k);

        virtual ~ActionCondition();

        virtual void action(ActionManager& mgr, MaybeDocument);

        StringKey<ActionCondition> key() const;

        void setEnabled(iscore::ActionManager& mgr, bool b);

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


using ActionConditionKey = StringKey<iscore::ActionCondition>;

template<typename T>
class EnableWhenSelectionContains;

#define ISCORE_DECLARE_SELECTED_OBJECT_CONDITION(Type) \
namespace iscore { template<> \
class EnableWhenSelectionContains<Type> final : \
        public iscore::SelectionActionCondition \
{ \
    public: \
        EnableWhenSelectionContains(): \
            iscore::SelectionActionCondition{static_key()} { } \
 \
        static iscore::ActionConditionKey static_key() \
        { return iscore::ActionConditionKey{ "SelectedObjectIs" #Type }; } \
 \
    private: \
        void action(iscore::ActionManager& mgr, iscore::MaybeDocument doc) override \
        { \
            if(!doc) \
            { \
                setEnabled(mgr, false); \
                return; \
            } \
 \
            const auto& sel = doc->selectionStack.currentSelection(); \
            auto res = any_of(sel, [] (auto obj) { return bool(dynamic_cast<const Type*>(obj.data())); }); \
 \
            setEnabled(mgr, res); \
        } \
}; }

template<typename T>
class EnableWhenFocusedObjectIs;
template<typename T>
class EnableWhenDocumentIs;
#define ISCORE_DECLARE_FOCUSED_OBJECT_CONDITION(Type) \
namespace iscore { template<> \
class EnableWhenFocusedObjectIs<Type> final : public iscore::FocusActionCondition   \
{                                                                                   \
    public:                                                                         \
        static iscore::ActionConditionKey static_key() { return iscore::ActionConditionKey{"FocusedObjectIs" #Type }; }  \
                                                                                    \
        EnableWhenFocusedObjectIs():                                                \
            iscore::FocusActionCondition{static_key()}                              \
        {                                                                           \
                                                                                    \
        }                                                                           \
                                                                                    \
    private:                                                                        \
        void action(iscore::ActionManager& mgr, iscore::MaybeDocument doc) override \
        {                                                                           \
            if(!doc)                                                                \
            {                                                                       \
                setEnabled(mgr, false);                                             \
                return;                                                             \
            }                                                                       \
                                                                                    \
            auto obj = doc->focus.get();                                            \
            if(!obj)                                                                \
            {                                                                       \
                setEnabled(mgr, false);                                             \
                return;                                                             \
            }                                                                       \
                                                                                 \
            if(dynamic_cast<const Type*>(obj))                                         \
            {                                                                       \
                setEnabled(mgr, true);                                              \
            }                                                                       \
        }                                                                           \
}; }

#define ISCORE_DECLARE_DOCUMENT_CONDITION(Type) \
namespace iscore { template<> \
class EnableWhenDocumentIs<Type> final : public iscore::DocumentActionCondition            \
{                                                                                    \
    public:                                                                          \
        static iscore::ActionConditionKey static_key() { return iscore::ActionConditionKey{"DocumentIs" #Type }; }        \
        EnableWhenDocumentIs():                  \
            iscore::DocumentActionCondition{static_key()}                            \
        {                                                                            \
                                                                                     \
        }                                                                            \
                                                                                     \
    private:                                                                         \
        void action(iscore::ActionManager& mgr, iscore::MaybeDocument doc) override  \
        {                                                                            \
            if(!doc)                                                                 \
            {                                                                        \
                setEnabled(mgr, false);                                              \
                return;                                                              \
            }                                                                        \
            auto model = iscore::IDocument::try_get<Type>(doc->document);            \
            setEnabled(mgr, bool(model));                                            \
        }                                                                            \
}; }

class ISCORE_LIB_BASE_EXPORT ActionManager :
        public QObject
{
    public:
        ActionManager();

        void insert(Action val);

        void insert(std::vector<Action> vals);

        auto& get() const { return m_container; }
        template<typename T>
        auto& action()
        { return m_container.at(MetaAction<T>::key()); }
        template<typename T>
        auto& action() const
        { return m_container.at(MetaAction<T>::key()); }

        void reset(Document* doc);

        void onDocumentChange(std::shared_ptr<ActionCondition> cond);
        void onFocusChange(std::shared_ptr<ActionCondition> cond);
        void onSelectionChange(std::shared_ptr<ActionCondition> cond);
        void onCustomChange(std::shared_ptr<ActionCondition> cond);

        const auto& documentConditions() const { return m_docConditions; }
        const auto& focusConditions() const { return m_focusConditions; }
        const auto& selectionConditions() const { return m_selectionConditions; }
        const auto& customConditions() const { return m_customConditions; }

        template<typename Condition_T, typename std::enable_if_t<std::is_base_of<DocumentActionCondition, Condition_T>::value, void*> = nullptr>
        auto& condition() const
        {
            return *m_docConditions.at(Condition_T::static_key());
        }
        template<typename Condition_T, typename std::enable_if_t<std::is_base_of<FocusActionCondition, Condition_T>::value, void*> = nullptr>
        auto& condition() const
        {
            return *m_focusConditions.at(Condition_T::static_key());
        }
        template<typename Condition_T, typename std::enable_if_t<std::is_base_of<SelectionActionCondition, Condition_T>::value, void*> = nullptr>
        auto& condition() const
        {
            return *m_selectionConditions.at(Condition_T::static_key());
        }
        template<typename Condition_T, typename std::enable_if_t<std::is_base_of<CustomActionCondition, Condition_T>::value, void*> = nullptr>
        auto& condition() const
        {
            return *m_customConditions.at(Condition_T::static_key());
        }

        template<typename Condition_T, typename std::enable_if_t<
                     !std::is_base_of<DocumentActionCondition, Condition_T>::value &&
                     !std::is_base_of<FocusActionCondition, Condition_T>::value &&
                     !std::is_base_of<SelectionActionCondition, Condition_T>::value &&
                     !std::is_base_of<CustomActionCondition, Condition_T>::value &&
                     std::is_base_of<ActionCondition, Condition_T>::value, void*> = nullptr>
        auto& condition() const
        {
            return *m_conditions.at(Condition_T::static_key());
        }
    private:
        void documentChanged(MaybeDocument doc);
        void focusChanged(MaybeDocument doc);
        void selectionChanged(MaybeDocument doc);
        void resetCustomActions(MaybeDocument doc);
        std::unordered_map<ActionKey, Action> m_container;

        // Conditions for the enablement of the actions
        std::unordered_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>> m_docConditions;
        std::unordered_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>> m_focusConditions;
        std::unordered_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>> m_selectionConditions;
        std::unordered_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>> m_customConditions;
        std::unordered_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>> m_conditions;

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

class ISCORE_LIB_BASE_EXPORT ContextMenuBuilder
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
class ISCORE_LIB_BASE_EXPORT MenuManager
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

class ISCORE_LIB_BASE_EXPORT ToolbarManager
{
    public:
        void insert(Toolbar val);

        void insert(std::vector<Toolbar> vals);

        auto& get() { return m_container; }
        auto& get() const { return m_container; }

    private:
        std::unordered_map<StringKey<Toolbar>, Toolbar> m_container;
};


struct GUIElements
{
        ActionContainer actions;
        std::vector<Menu> menus;
        std::vector<Toolbar> toolbars;
};

}

#define ISCORE_DECLARE_ACTION(ActionName, Group, Shortcut) \
namespace Actions { struct ActionName; }\
namespace iscore { \
template<> \
struct MetaAction<Actions::ActionName> \
{ \
  static iscore::Action make(QAction* ptr)  \
  { \
    return iscore::Action{ptr, key(), iscore::ActionGroupKey{#Group}, Shortcut}; \
  } \
\
  static iscore::ActionKey key() \
  { \
    return iscore::ActionKey{#ActionName} ; \
  } \
}; }
