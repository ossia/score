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
        QList<const QObject*> sourceElements,
        const iscore::DocumentContext& doc,
        QWidget* parentWidget) const
{
 //TODO !!
    return new Inspector::InspectorWidgetBase{
        static_cast<const EventModel&>(*sourceElements.first()),
                doc,
                parentWidget};
}

bool EventInspectorFactory::matches(QList<const QObject*> objects) const
{
    return dynamic_cast<const EventModel*>(objects.first());
}
}
