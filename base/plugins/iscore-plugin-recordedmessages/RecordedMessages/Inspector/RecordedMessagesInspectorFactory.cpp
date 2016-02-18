#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "RecordedMessagesInspectorFactory.hpp"
#include "RecordedMessages/RecordedMessagesProcessModel.hpp"
#include "RecordedMessagesInspectorWidget.hpp"

class InspectorWidgetBase;
class RecordedMessagesProcessModel;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace RecordedMessages
{

InspectorFactory::InspectorFactory()
{

}

InspectorFactory::~InspectorFactory()
{

}

ProcessInspectorWidgetDelegate* InspectorFactory::make(
        const Process::ProcessModel& process,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new InspectorWidget{
        static_cast<const RecordedMessages::ProcessModel&>(process),
                doc,
                parent};
}

bool InspectorFactory::matches(const Process::ProcessModel& process) const
{
    return dynamic_cast<const RecordedMessages::ProcessModel*>(&process);
}
}
