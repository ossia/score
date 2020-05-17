#pragma once
#include <score/actions/Action.hpp>
namespace Process
{
template <typename T>
struct EnableWhenFocusedProcessIs;
}

#define SCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Type)                                \
  namespace Process                                                                  \
  {                                                                                  \
  template <>                                                                        \
  struct EnableWhenFocusedProcessIs<Type> final : public score::FocusActionCondition \
  {                                                                                  \
  public:                                                                            \
    static score::ActionConditionKey static_key()                                    \
    {                                                                                \
      return score::ActionConditionKey{"FocusedProcessIs" #Type};                    \
    }                                                                                \
                                                                                     \
    EnableWhenFocusedProcessIs() : score::FocusActionCondition{static_key()} { }     \
                                                                                     \
  private:                                                                           \
    void action(score::ActionManager& mgr, score::MaybeDocument doc) override        \
    {                                                                                \
      if (!doc)                                                                      \
      {                                                                              \
        setEnabled(mgr, false);                                                      \
        return;                                                                      \
      }                                                                              \
                                                                                     \
      auto obj = doc->focus.get();                                                   \
      if (!obj)                                                                      \
      {                                                                              \
        setEnabled(mgr, false);                                                      \
        return;                                                                      \
      }                                                                              \
                                                                                     \
      setEnabled(mgr, bool(dynamic_cast<const Type*>(obj)));                         \
    }                                                                                \
  };                                                                                 \
  }
