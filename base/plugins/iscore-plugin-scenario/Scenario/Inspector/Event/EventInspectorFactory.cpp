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

const QList<QString>&EventInspectorFactory::key_impl() const
{
    static const QList<QString> list{"EventModel"};
    return list;
}
}
