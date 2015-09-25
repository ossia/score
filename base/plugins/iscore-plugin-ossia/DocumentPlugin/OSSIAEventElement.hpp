#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <State/Expression.hpp>
#include <API/Headers/Editor/TimeEvent.h>
#include <memory>
class DeviceList;
class EventModel;
namespace OSSIA
{
    class TimeEvent;
}
class OSSIAEventElement : public QObject
{
    public:
        OSSIAEventElement(
                std::shared_ptr<OSSIA::TimeEvent> event,
                const EventModel& element,
                const DeviceList& deviceList,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeEvent> event() const;

        bool event(QEvent* ev) override { return QObject::event(ev); }

    private:
        void on_conditionChanged(const iscore::Condition& c);

        const EventModel& m_iscore_event;
        std::shared_ptr<OSSIA::TimeEvent> m_event;

        const DeviceList& m_deviceList;
};
