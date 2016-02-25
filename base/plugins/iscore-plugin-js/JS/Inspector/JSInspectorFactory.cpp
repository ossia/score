#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "JSInspectorFactory.hpp"
#include "JS/JSProcessModel.hpp"
#include "JS/StateProcess.hpp"
#include "JSInspectorWidget.hpp"

class InspectorWidgetBase;
class JSProcessModel;
class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace JS
{

InspectorFactory::InspectorFactory()
{

}

InspectorFactory::~InspectorFactory()
{

}

Process::InspectorWidgetDelegate* InspectorFactory::make(
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




StateInspectorFactory::StateInspectorFactory()
{

}

StateInspectorFactory::~StateInspectorFactory()
{

}

Process::StateProcessInspectorWidgetDelegate* StateInspectorFactory::make(
        const Process::StateProcess& process,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new StateInspectorWidget{
        static_cast<const JS::StateProcess&>(process),
                doc,
                parent};
}

bool StateInspectorFactory::matches(const Process::StateProcess& process) const
{
    return dynamic_cast<const JS::StateProcess*>(&process);
}
}
