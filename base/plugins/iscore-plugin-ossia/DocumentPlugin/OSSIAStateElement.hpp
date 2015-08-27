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
        OSSIAStateElement(
                const StateModel& element,
                std::shared_ptr<OSSIA::State> root,
                QObject* parent);

        const StateModel& iscoreState() const;
        std::shared_ptr<OSSIA::State> rootState() const
        { return m_ossia_rootState; }

    private:
        const StateModel& m_iscore_state;
        std::shared_ptr<OSSIA::State> m_ossia_rootState;
};
