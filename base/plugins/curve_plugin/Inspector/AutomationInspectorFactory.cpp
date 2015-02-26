#include "AutomationInspectorFactory.hpp"
#include "AutomationInspectorWidget.hpp"
#include "../Automation/AutomationModel.hpp"

//using namespace iscore;

InspectorWidgetBase* AutomationInspectorFactory::makeWidget(QObject* sourceElement)
{
    return new AutomationInspectorWidget(static_cast<AutomationModel*>(sourceElement));

}

InspectorWidgetBase* AutomationInspectorFactory::makeWidget(QList<QObject*> sourceElements)
{
    // @todo make a tabbed view when there is a list.
    return new AutomationInspectorWidget(static_cast<AutomationModel*>(sourceElements.at(0)));
}
