#include <QString>

#include <Automation/AutomationProcessMetadata.hpp>
#include "AutomationInspectorFactory.hpp"
#include "AutomationInspectorWidget.hpp"
#include <Automation/AutomationModel.hpp>
class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore {
class Document;
}

ProcessInspectorWidgetDelegate* AutomationInspectorFactory::make(
        const Process::ProcessModel& process,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new AutomationInspectorWidget{
        static_cast<const AutomationModel&>(process), doc, parent};
}

bool AutomationInspectorFactory::matches(const Process::ProcessModel& process) const
{
    return dynamic_cast<const AutomationModel*>(&process);
}
