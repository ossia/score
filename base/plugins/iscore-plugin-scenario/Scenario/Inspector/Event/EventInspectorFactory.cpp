#include <Scenario/Document/Event/EventModel.hpp>
#include <qstring.h>

#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

InspectorWidgetBase* EventInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parentWidget)
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
