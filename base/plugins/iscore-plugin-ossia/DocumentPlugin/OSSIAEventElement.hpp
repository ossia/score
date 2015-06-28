#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>

#include <memory>
class EventModel;
namespace OSSIA
{
    class TimeEvent;
}
class OSSIAEventElement : public iscore::ElementPluginModel
{
    public:
        static constexpr iscore::ElementPluginModelType staticPluginId() { return 2; }
        OSSIAEventElement(
                std::shared_ptr<OSSIA::TimeEvent> event,
                const EventModel* element,
                QObject* parent);

        std::shared_ptr<OSSIA::TimeEvent> event() const;

        iscore::ElementPluginModelType elementPluginId() const;

        iscore::ElementPluginModel*clone(
                const QObject* element,
                QObject* parent) const override;

        void serialize(const VisitorVariant&) const;

    private:
        std::shared_ptr<OSSIA::TimeEvent> m_event;
};
