#include "AutomationStateInspectorFactory.hpp"
#include "AutomationStateInspector.hpp"
#include "State/AutomationState.hpp"

InspectorWidgetBase* AutomationStateInspectorFactory::makeWidget(QObject* sourceElement, QWidget* parent)
{
    return new AutomationStateInspector(static_cast<AutomationState*>(sourceElement), parent);
}
