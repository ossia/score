#include <Scenario/Document/Event/EventModel.hpp>
#include <QString>

#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
Inspector::InspectorWidgetBase* EventInspectorFactory::makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parentWidget) const
{
    return new EventInspectorWidget{
        static_cast<const EventModel&>(sourceElement),
                doc,
                parentWidget};
}

bool EventInspectorFactory::matches(const QObject& object) const
{
    return dynamic_cast<const EventModel*>(&object);
}
}
