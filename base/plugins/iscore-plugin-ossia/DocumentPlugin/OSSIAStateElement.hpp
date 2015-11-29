#pragma once
#include <qobject.h>
#include <memory>

class DeviceList;
class StateModel;

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


        void recreate();

        void clear();

    private:
        void on_stateUpdated();

        const StateModel& m_iscore_state;
        std::shared_ptr<OSSIA::State> m_ossia_state;

        const DeviceList& m_deviceList;
};
