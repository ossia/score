#pragma once
#include <QAction>
#include <QMenu>
#include <QString>
#include <QToolBar>
#include <memory>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>

/**
 * \file Action.hpp
 *
 * * Conditions
 * * Integration with menus, toolbars, etc.
 * * TODO solve the problem of a "single" action used in multiple contexts
 * e.g. copy-paste.
 * - Steps :
 *   * declaring the conditions
 *   * declaring the action types
 *   * instantiating the conditions
 *   * instantiating the action types
 */

class IdentifiedObjectAbstract;
namespace score
{
struct DocumentContext;
class ActionGroup;
using ActionGroupKey = StringKey<ActionGroup>;

/**
 * @brief The ActionGroup class
 * A semantic group of actions : for instance, all
 * the actions related to audio recording, etc.
 * This is to be used for documentation purposes, for instance
 * on a potential keyboard shortcut widget.
 */
class SCORE_LIB_BASE_EXPORT ActionGroup
{
public:
  ActionGroup(QString prettyName, ActionGroupKey key);

  /**
   * @brief prettyName
   * @return A name shown to the user in the UI.
   */
  QString prettyName() const;

  ActionGroupKey key() const;

private:
  QString m_name;
  ActionGroupKey m_key;
};
class Action;
using ActionKey = StringKey<Action>;

/**
 * @brief The Action class
 *
 * An action is a wrapper class for QAction, which
 * adds informations that will be useful to allow to change
 * the keyboard shortcuts :
 * - Default shortcuts
 * - Group
 *
 * Actions should be part of the type systems as much as possible
 * to prevent errors.
 *
 * To allow this, the \ref score::MetaAction template shall be
 * specialized. The macro \ref SCORE_DECLARE_ACTION is provided to
 * do this easily.
 *
 * The ActionKey should be unique across the whole software.
 */
class SCORE_LIB_BASE_EXPORT Action
{
public:
  Action(
      QAction* act,
      QString text,
      ActionKey key,
      ActionGroupKey k,
      const QKeySequence& defaultShortcut);
  Action(
      QAction* act,
      QString text,
      ActionKey key,
      ActionGroupKey k,
      const QKeySequence& defaultShortcut,
      const QKeySequence& defaultShortcut2);

  Action(
      QAction* act,
      QString text,
      const char* key,
      const char* group_key,
      const QKeySequence& defaultShortcut);

  ActionKey key() const;

  QAction* action() const;

  QKeySequence shortcut();
  void setShortcut(const QKeySequence& shortcut);

  QKeySequence defaultShortcut();

private:
  void updateTexts();

  QAction* m_impl{};
  QString m_text;
  ActionKey m_key;
  ActionGroupKey m_groupKey;
  QKeySequence m_default;
  QKeySequence m_current;
};

template <typename Action_T>
struct MetaAction
{
};

/**
 * @brief The ActionContainer struct
 *
 * Where the actions will be registered.
 * Mainly used while creating \ref score::GUIElements :
 *
 * \code
 * auto act = new QAction{...};
 * connect(act, &QAction::toggled, ...);
 *
 * score::GUIElements e;
 * e.actions.add<MyActionType>(act);
 * \endcode
 */
struct ActionContainer
{
public:
  std::vector<Action> container;

  template <typename Action_T>
  void add(QAction* ptr)
  {
    SCORE_ASSERT(
        ossia::find_if(
            container,
            [](auto ac) { return ac.key() == MetaAction<Action_T>::key(); })
        == container.end());
    container.emplace_back(MetaAction<Action_T>::make(ptr));
  }
};

/**
 * @brief The ActionCondition struct
 *
 * Base class for conditions on actions.
 * This mechanism allows to enable / disable a set of actions
 * when a specific event occurs in the software.
 *
 * For instance, some actions should be enabled only when some particular
 * group of elements are enabled.
 *
 * \see score::ActionManager
 */
struct SCORE_LIB_BASE_EXPORT ActionCondition
{
  ActionCondition(StringKey<ActionCondition> k);

  virtual ~ActionCondition();

  /**
   * @brief action This function will be called whenever a particular event
   * happen.
   *
   * The default implementation does nothing.
   */
  virtual void action(ActionManager& mgr, MaybeDocument);

  /**
   * @brief setEnabled Sets the state of all the matching actions
   *
   * This function should be called by implementations of the
   * \ref ActionCondition::action function.
   *
   * It will enable or disable all the actions registered for this condition.
   */
  void setEnabled(score::ActionManager& mgr, bool b);

  /**
   * @brief add Register an action for this condition.
   *
   * \see \ref SCORE_DECLARE_ACTION
   */
  template <typename Action_T>
  void add()
  {
    SCORE_ASSERT(
        ossia::find_if(
            m_actions,
            [](auto ac) { return ac == MetaAction<Action_T>::key(); })
        == m_actions.end());

    m_actions.emplace_back(MetaAction<Action_T>::key());
  }

  StringKey<ActionCondition> key() const;

private:
  StringKey<ActionCondition> m_key;
  std::vector<ActionKey> m_actions;
};

/**
 * @brief The DocumentActionCondition struct
 *
 * Will be checked when the document changes
 */
struct SCORE_LIB_BASE_EXPORT DocumentActionCondition : public ActionCondition
{
  using ActionCondition::ActionCondition;
  ~DocumentActionCondition();
};

/**
 * @brief The EnableActionIfDocument struct
 *
 * Enables its actions if there is an active document.
 */
struct SCORE_LIB_BASE_EXPORT EnableActionIfDocument final
    : public DocumentActionCondition
{
  EnableActionIfDocument() : DocumentActionCondition{static_key()}
  {
  }

  ~EnableActionIfDocument();

  static StringKey<ActionCondition> static_key()
  {
    return StringKey<ActionCondition>{"EnableActionIfDocument"};
  }

  void action(ActionManager& mgr, MaybeDocument doc) override;
};

/**
 * @brief The FocusActionCondition struct
 *
 * Will be checked when the focused object changes
 */
struct SCORE_LIB_BASE_EXPORT FocusActionCondition : public ActionCondition
{
  using ActionCondition::ActionCondition;
  ~FocusActionCondition();
};

/**
 * @brief The SelectionActionCondition struct
 *
 * Will be checked when the selection changes
 */
struct SCORE_LIB_BASE_EXPORT SelectionActionCondition : public ActionCondition
{
  using ActionCondition::ActionCondition;
  ~SelectionActionCondition();
};

/**
 * @brief The CustomActionCondition struct
 *
 * Will be checked when the \ref CustomActionCondition::changed signal is
 * emitted
 */
struct SCORE_LIB_BASE_EXPORT CustomActionCondition
    : public QObject
    , public ActionCondition
{
  W_OBJECT(CustomActionCondition)

public:
  using ActionCondition::ActionCondition;
  ~CustomActionCondition();

  void changed(bool b) W_SIGNAL(changed, b)
};

using ActionConditionKey = StringKey<score::ActionCondition>;

template <typename T>
class EnableWhenSelectionContains;
template <typename T>
class EnableWhenFocusedObjectIs;
template <typename T>
class EnableWhenDocumentIs;

/**
 * \macro SCORE_DECLARE_SELECTED_OBJECT_CONDITION
 *
 * Use this macro to declare a new condition that will be enabled
 * whenever an object of the given type is selected.
 *
 * e.g. SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Foo) will
 * declare a condition that will enable its actions whenever an object of type
 * Foo is
 * selected.
 *
 * \warning This macro must be used outside of any namespace.
 */
#define SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Type)                         \
  namespace score                                                             \
  {                                                                           \
  template <>                                                                 \
  class EnableWhenSelectionContains<Type> final                               \
      : public score::SelectionActionCondition                                \
  {                                                                           \
  public:                                                                     \
    EnableWhenSelectionContains()                                             \
        : score::SelectionActionCondition{static_key()}                       \
    {                                                                         \
    }                                                                         \
                                                                              \
    static score::ActionConditionKey static_key()                             \
    {                                                                         \
      return score::ActionConditionKey{"SelectedObjectIs" #Type};             \
    }                                                                         \
                                                                              \
  private:                                                                    \
    void action(score::ActionManager& mgr, score::MaybeDocument doc) override \
    {                                                                         \
      if (!doc)                                                               \
      {                                                                       \
        setEnabled(mgr, false);                                               \
        return;                                                               \
      }                                                                       \
                                                                              \
      const auto& sel = doc->selectionStack.currentSelection();               \
      auto res = ossia::any_of(sel, [](auto obj) {                            \
        return bool(dynamic_cast<const Type*>(obj.data()));                   \
      });                                                                     \
                                                                              \
      setEnabled(mgr, res);                                                   \
    }                                                                         \
  };                                                                          \
  }

/**
 * \macro SCORE_DECLARE_FOCUSED_OBJECT_CONDITION
 *
 * Use this macro to declare a new condition that will be enabled
 * whenever an object of the given type is focused.
 *
 * e.g. SCORE_DECLARE_FOCUSED_OBJECT_CONDITION(Foo) will
 * declare a condition that will enable its actions whenever an object of type
 * Foo is
 * focused.
 *
 * \warning This macro must be used outside of any namespace.
 */
#define SCORE_DECLARE_FOCUSED_OBJECT_CONDITION(Type)                          \
  namespace score                                                             \
  {                                                                           \
  template <>                                                                 \
  class EnableWhenFocusedObjectIs<Type> final                                 \
      : public score::FocusActionCondition                                    \
  {                                                                           \
  public:                                                                     \
    static score::ActionConditionKey static_key()                             \
    {                                                                         \
      return score::ActionConditionKey{"FocusedObjectIs" #Type};              \
    }                                                                         \
                                                                              \
    EnableWhenFocusedObjectIs() : score::FocusActionCondition{static_key()}   \
    {                                                                         \
    }                                                                         \
                                                                              \
  private:                                                                    \
    void action(score::ActionManager& mgr, score::MaybeDocument doc) override \
    {                                                                         \
      if (!doc)                                                               \
      {                                                                       \
        setEnabled(mgr, false);                                               \
        return;                                                               \
      }                                                                       \
                                                                              \
      auto obj = doc->focus.get();                                            \
      if (!obj)                                                               \
      {                                                                       \
        setEnabled(mgr, false);                                               \
        return;                                                               \
      }                                                                       \
                                                                              \
      if (dynamic_cast<const Type*>(obj))                                     \
      {                                                                       \
        setEnabled(mgr, true);                                                \
      }                                                                       \
    }                                                                         \
  };                                                                          \
  }

/**
 * \macro SCORE_DECLARE_DOCUMENT_CONDITION
 *
 * Use this macro to declare a new condition that will be enabled
 * whenever a document of the given type is brought forward, or open.
 *
 * e.g. SCORE_DECLARE_DOCUMENT_CONDITION(Foo) will
 * declare a condition that will enable its actions whenever a document of type
 * Foo is
 * opened (by clicking in its name on the document tab for instance).
 *
 * \warning This macro must be used outside of any namespace.
 */
#define SCORE_DECLARE_DOCUMENT_CONDITION(Type)                                \
  namespace score                                                             \
  {                                                                           \
  template <>                                                                 \
  class EnableWhenDocumentIs<Type> final                                      \
      : public score::DocumentActionCondition                                 \
  {                                                                           \
  public:                                                                     \
    static score::ActionConditionKey static_key()                             \
    {                                                                         \
      return score::ActionConditionKey{"DocumentIs" #Type};                   \
    }                                                                         \
    EnableWhenDocumentIs() : score::DocumentActionCondition{static_key()}     \
    {                                                                         \
    }                                                                         \
                                                                              \
  private:                                                                    \
    void action(score::ActionManager& mgr, score::MaybeDocument doc) override \
    {                                                                         \
      if (!doc)                                                               \
      {                                                                       \
        setEnabled(mgr, false);                                               \
        return;                                                               \
      }                                                                       \
      auto model = score::IDocument::try_get<Type>(doc->document);            \
      setEnabled(mgr, bool(model));                                           \
    }                                                                         \
  };                                                                          \
  }
}

#define SCORE_DECLARE_ACTION(ActionName, Text, Group, Shortcut)      \
  namespace Actions                                                  \
  {                                                                  \
  struct ActionName;                                                 \
  }                                                                  \
  namespace score                                                    \
  {                                                                  \
  template <>                                                        \
  struct MetaAction<Actions::ActionName>                             \
  {                                                                  \
    static score::Action make(QAction* ptr)                          \
    {                                                                \
      return score::Action{ptr, QObject::tr(Text), key(),            \
                           score::ActionGroupKey{#Group}, Shortcut}; \
    }                                                                \
                                                                     \
    static score::ActionKey key()                                    \
    {                                                                \
      return score::ActionKey{#ActionName};                          \
    }                                                                \
  };                                                                 \
  }

#define SCORE_DECLARE_ACTION_2S(                                     \
    ActionName, Text, Group, Shortcut1, Shortcut2)                   \
  namespace Actions                                                  \
  {                                                                  \
  struct ActionName;                                                 \
  }                                                                  \
  namespace score                                                    \
  {                                                                  \
  template <>                                                        \
  struct MetaAction<Actions::ActionName>                             \
  {                                                                  \
    static score::Action make(QAction* ptr)                          \
    {                                                                \
      return score::Action{ptr,       QObject::tr(Text),             \
                           key(),     score::ActionGroupKey{#Group}, \
                           Shortcut1, Shortcut2};                    \
    }                                                                \
                                                                     \
    static score::ActionKey key()                                    \
    {                                                                \
      return score::ActionKey{#ActionName};                          \
    }                                                                \
  };                                                                 \
  }
