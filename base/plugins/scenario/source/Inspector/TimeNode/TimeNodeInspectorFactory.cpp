#include "TimeNodeInspectorFactory.hpp"
#include "TimeNodeInspectorWidget.hpp"

#include <Document/TimeNode/TimeNodeModel.hpp>

//using namespace iscore;

InspectorWidgetBase* TimeNodeInspectorFactory::makeWidget(QObject* sourceElement)
{
    auto timeNode = static_cast<TimeNodeModel*>(sourceElement);
    return new TimeNodeInspectorWidget(timeNode);
}
