#pragma once
#include <QObject>
#include <memory>
#include <iscore_plugin_ossia_export.h>

namespace Device
{
class DeviceList;
}
namespace ossia
{
    class time_event;
}
namespace Scenario
{
class EventModel;
}

namespace RecreateOnPlay
{
class ISCORE_PLUGIN_OSSIA_EXPORT EventElement final : public QObject
{
        Q_OBJECT
    public:
        EventElement(
                std::shared_ptr<ossia::time_event> event,
                const Scenario::EventModel& element,
                const Device::DeviceList& deviceList,
                QObject* parent);

        std::shared_ptr<ossia::time_event> OSSIAEvent() const;
        const Scenario::EventModel& iscoreEvent() const
        { return m_iscore_event; }

    signals:
        void happened();

    private:
        const Scenario::EventModel& m_iscore_event;
        std::shared_ptr<ossia::time_event> m_ossia_event;
        const Device::DeviceList& m_deviceList;
};
}
