#include "AutomationStateInspectorFactory.hpp"
#include "AutomationStateInspector.hpp"
#include "State/AutomationState.hpp"

InspectorWidgetBase* AutomationStateInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    return new AutomationStateInspector{
                safe_cast<const AutomationState&>(sourceElement),
                doc,
                parent};
}
