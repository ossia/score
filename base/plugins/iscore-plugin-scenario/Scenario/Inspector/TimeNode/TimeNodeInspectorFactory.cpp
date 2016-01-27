#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <QString>

#include "TimeNodeInspectorFactory.hpp"
#include "TimeNodeInspectorWidget.hpp"

class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
Inspector::InspectorWidgetBase* TimeNodeInspectorFactory::makeWidget(
        const QObject& sourceElement,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    auto& timeNode = static_cast<const TimeNodeModel&>(sourceElement);
    return new TimeNodeInspectorWidget{timeNode, doc, parent};
}

bool TimeNodeInspectorFactory::matches(const QObject& object) const
{
    return dynamic_cast<const TimeNodeModel*>(&object);
}
}
