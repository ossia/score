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

namespace JS
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
        static_cast<const JS::ProcessModel&>(process),
                doc,
                parent};
}

bool InspectorFactory::matches(const Process::ProcessModel& process) const
{
    return dynamic_cast<const JS::ProcessModel*>(&process);
}
}
