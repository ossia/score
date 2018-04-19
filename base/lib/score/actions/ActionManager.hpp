#pragma once
#include <score/actions/Action.hpp>
#include <score/tools/std/HashMap.hpp>

namespace score
{
template <typename T1, typename T2>
using enable_if_base_of
    = std::enable_if<std::is_base_of<T1, T2>::value, void*>;

/**
 * @brief The ActionManager class
 *
 * Keeps track of the \ref Action%s registered in the software,
 * and handles the action triggering mechanism for each condition.
 *
 * Accessible through an \ref score::ApplicationContext.
 */
class SCORE_LIB_BASE_EXPORT ActionManager : public QObject
{
public:
  ActionManager();
  ~ActionManager();

  void insert(Action val);

  void insert(std::vector<Action> vals);

  auto& get()
  {
    return m_container;
  }
  auto& get() const
  {
    return m_container;
  }
  template <typename T>
  auto& action()
  {
    return m_container.at(MetaAction<T>::key());
  }
  template <typename T>
  auto& action() const
  {
    return m_container.at(MetaAction<T>::key());
  }

  void reset(Document* doc);

  void onDocumentChange(std::shared_ptr<ActionCondition> cond);
  void onFocusChange(std::shared_ptr<ActionCondition> cond);
  void onSelectionChange(std::shared_ptr<ActionCondition> cond);
  void onCustomChange(std::shared_ptr<ActionCondition> cond);

  const auto& documentConditions() const
  {
    return m_docConditions;
  }
  const auto& focusConditions() const
  {
    return m_focusConditions;
  }
  const auto& selectionConditions() const
  {
    return m_selectionConditions;
  }
  const auto& customConditions() const
  {
    return m_customConditions;
  }

  template <
      typename Condition_T,
      typename enable_if_base_of<DocumentActionCondition, Condition_T>::type
      = nullptr>
  auto& condition() const
  {
    return *m_docConditions.at(Condition_T::static_key());
  }
  template <
      typename Condition_T,
      typename enable_if_base_of<FocusActionCondition, Condition_T>::type
      = nullptr>
  auto& condition() const
  {
    return *m_focusConditions.at(Condition_T::static_key());
  }
  template <
      typename Condition_T,
      typename enable_if_base_of<SelectionActionCondition, Condition_T>::type
      = nullptr>
  auto& condition() const
  {
    return *m_selectionConditions.at(Condition_T::static_key());
  }
  template <
      typename Condition_T,
      typename enable_if_base_of<CustomActionCondition, Condition_T>::type
      = nullptr>
  auto& condition() const
  {
    return *m_customConditions.at(Condition_T::static_key());
  }

  template <
      typename Condition_T,
      typename std::enable_if<
          !std::is_base_of<DocumentActionCondition, Condition_T>::value
              && !std::is_base_of<FocusActionCondition, Condition_T>::value
              && !std::is_base_of<SelectionActionCondition, Condition_T>::value
              && !std::is_base_of<CustomActionCondition, Condition_T>::value
              && std::is_base_of<ActionCondition, Condition_T>::value,
          void*>::type
      = nullptr>
  auto& condition() const
  {
    return *m_conditions.at(Condition_T::static_key());
  }

private:
  void documentChanged(MaybeDocument doc);
  void focusChanged(MaybeDocument doc);
  void selectionChanged(MaybeDocument doc);
  void resetCustomActions(MaybeDocument doc);

  score::hash_map<ActionKey, Action> m_container;

  // Conditions for the enablement of the actions
  score::hash_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>>
      m_docConditions;
  score::hash_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>>
      m_focusConditions;
  score::hash_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>>
      m_selectionConditions;
  score::hash_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>>
      m_customConditions;
  score::hash_map<StringKey<ActionCondition>, std::shared_ptr<ActionCondition>>
      m_conditions;

  QMetaObject::Connection focusConnection;
  QMetaObject::Connection selectionConnection;
};
}
