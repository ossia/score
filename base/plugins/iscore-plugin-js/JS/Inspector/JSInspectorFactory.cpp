#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "JSInspectorFactory.hpp"
#include "JS/JSProcessModel.hpp"
#include "JSInspectorWidget.hpp"

class InspectorWidgetBase;
class JSProcessModel;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

//using namespace iscore;

JSInspectorFactory::JSInspectorFactory()
{

}

JSInspectorFactory::~JSInspectorFactory()
{

}

ProcessInspectorWidgetDelegate* JSInspectorFactory::make(
        const Process::ProcessModel& process,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new JSInspectorWidget{
        static_cast<const JS::ProcessModel&>(process),
                doc,
                parent};
}

bool JSInspectorFactory::matches(const Process::ProcessModel& process) const
{
    return dynamic_cast<const JS::ProcessModel*>(&process);
}
