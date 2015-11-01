#pragma once
#include <QObject>
#include <memory>
#include <unordered_map>
class StateModel;
class DeviceList;
namespace OSSIA
{
    class State;
}

class OSSIAStateElement final : public QObject
{
    public:
        OSSIAStateElement(
                const StateModel& element,
                std::shared_ptr<OSSIA::State> root,
                const DeviceList& deviceList,
                QObject* parent);

        const StateModel& iscoreState() const;
        std::shared_ptr<OSSIA::State> OSSIAState() const
        { return m_ossia_state; }

    private:
        void on_stateUpdated();

        const StateModel& m_iscore_state;
        std::shared_ptr<OSSIA::State> m_ossia_state;

        const DeviceList& m_deviceList;
};
