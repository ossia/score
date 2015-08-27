#include "AutomationInspectorFactory.hpp"
#include "AutomationInspectorWidget.hpp"
#include "../Automation/AutomationModel.hpp"

//using namespace iscore;

InspectorWidgetBase* AutomationInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    return new AutomationInspectorWidget{
                static_cast<const AutomationModel&>(sourceElement),
                doc,
                parent};
}
