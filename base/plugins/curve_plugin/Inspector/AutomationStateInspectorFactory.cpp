#include "AutomationStateInspectorFactory.hpp"
#include "AutomationStateInspector.hpp"
#include "State/AutomationState.hpp"

InspectorWidgetBase* AutomationStateInspectorFactory::makeWidget(
        const QObject* sourceElement,
        QWidget* parent)
{
    return new AutomationStateInspector(static_cast<const AutomationState*>(sourceElement), parent);
}
