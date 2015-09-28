#pragma once
#include <API/Headers/Editor/TimeEvent.h>
#include <QObject>
#include <memory>
#include <State/State.hpp>
#include <unordered_map>
class StateModel;
class DeviceList;
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
                const DeviceList& deviceList,
                QObject* parent);

        const StateModel& iscoreState() const;
        std::shared_ptr<OSSIA::State> rootState() const
        { return m_ossia_rootState; }

    private:
        void on_stateUpdated();

        const StateModel& m_iscore_state;
        std::shared_ptr<OSSIA::State> m_ossia_rootState;

        const DeviceList& m_deviceList;
};
