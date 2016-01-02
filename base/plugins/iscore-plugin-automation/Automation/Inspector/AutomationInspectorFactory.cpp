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

namespace Automation
{
ProcessInspectorWidgetDelegate* InspectorFactory::make(
        const Process::ProcessModel& process,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new InspectorWidget{
        static_cast<const ProcessModel&>(process), doc, parent};
}

bool InspectorFactory::matches(const Process::ProcessModel& process) const
{
    return dynamic_cast<const ProcessModel*>(&process);
}
}
