#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore
namespace Scenario
{
class EventInspectorFactory final : public Inspector::InspectorWidgetFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("f71c6643-cf85-4e35-a76a-b1d365416f33")
    public:
        EventInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        Inspector::InspectorWidgetBase* makeWidget(
                const QObject& sourceElement,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;

        bool matches(const QObject& object) const override;

};
}
