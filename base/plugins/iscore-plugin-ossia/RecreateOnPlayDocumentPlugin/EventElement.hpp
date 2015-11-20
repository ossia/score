#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <State/Expression.hpp>
#include <memory>
class DeviceList;
class EventModel;
namespace OSSIA
{
    class TimeEvent;
}

namespace RecreateOnPlay
{
class EventElement final : public QObject
{
    public:
        EventElement(
                std::shared_ptr<OSSIA::TimeEvent> event,
                const EventModel& element,
                const DeviceList& deviceList,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeEvent> OSSIAEvent() const;
        const EventModel& iscoreEvent() const
        { return m_iscore_event; }

    private:
        const EventModel& m_iscore_event;
        std::shared_ptr<OSSIA::TimeEvent> m_ossia_event;
        const DeviceList& m_deviceList;
};
}
