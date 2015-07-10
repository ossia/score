#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>

#include <memory>
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
                QObject* parent);

        std::shared_ptr<OSSIA::TimeEvent> event() const;


    private:
        std::shared_ptr<OSSIA::TimeEvent> m_event;
};
