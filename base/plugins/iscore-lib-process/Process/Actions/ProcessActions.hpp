#pragma once
#include <iscore/actions/Action.hpp>
namespace Process
{
template<typename T>
struct EnableWhenFocusedProcessIs;
}

#define ISCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Type)                              \
namespace Process { template<>                                                                \
struct EnableWhenFocusedProcessIs<Type> final : public iscore::FocusActionCondition       \
{                                                                                   \
    public:                                                                         \
    static iscore::ActionConditionKey static_key() { return iscore::ActionConditionKey{"FocusedProcessIs" #Type }; }     \
                                                                                    \
    EnableWhenFocusedProcessIs():                                                   \
        iscore::FocusActionCondition{static_key()}                                  \
    {                                                                               \
                                                                                    \
    }                                                                               \
                                                                                    \
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
            auto layer = dynamic_cast<const Process::LayerModel*>(obj);             \
            if(!layer)                                                              \
            {                                                                       \
                setEnabled(mgr, false);                                             \
                return;                                                             \
            }                                                                       \
                                                                                    \
            setEnabled(mgr, bool(dynamic_cast<Type*>(&layer->processModel())));     \
        }                                                                           \
}; }
