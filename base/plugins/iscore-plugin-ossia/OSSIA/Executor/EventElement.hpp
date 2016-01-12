#pragma once
#include <QObject>
#include <memory>
#include <iscore_plugin_ossia_export.h>

class DeviceList;

namespace OSSIA
{
    class TimeEvent;
}
namespace Scenario
{
class EventModel;
}

namespace RecreateOnPlay
{
class ISCORE_PLUGIN_OSSIA_EXPORT EventElement final : public QObject
{
    public:
        EventElement(
                std::shared_ptr<OSSIA::TimeEvent> event,
                const Scenario::EventModel& element,
                const DeviceList& deviceList,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeEvent> OSSIAEvent() const;
        const Scenario::EventModel& iscoreEvent() const
        { return m_iscore_event; }

    private:
        const Scenario::EventModel& m_iscore_event;
        std::shared_ptr<OSSIA::TimeEvent> m_ossia_event;
        const DeviceList& m_deviceList;
};
}
