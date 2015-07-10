#pragma once
#include <API/Headers/Editor/TimeEvent.h>
#include <QObject>
#include <memory>
#include <State/State.hpp>
#include <unordered_map>
class StateModel;
namespace OSSIA
{
    class State;
}

class OSSIAStateElement : public QObject
{
    public:
        using StateMap = std::unordered_map<iscore::State, std::shared_ptr<OSSIA::State>, state_hash>;
        OSSIAStateElement(
                const StateModel& element,
                QObject* parent);

        const StateModel& iscoreState() const;
        const StateMap &states() const;

        void addState(const iscore::State& is, std::shared_ptr<OSSIA::State>);
        void removeState(const iscore::State &);

    private:
        StateMap m_states;
        const StateModel& m_iscore_state;
};
