#include "Process.hpp"

#include <Process/Process.hpp>

namespace RemoteControl::WS
{

ProcessComponent::ProcessComponent(
    Process::ProcessModel& process, DocumentPlugin& doc, const QString& name,
    QObject* parent)
    : Process::GenericProcessComponent<DocumentPlugin>{process, doc, name, parent}
{
}

ProcessComponent::~ProcessComponent() { }

ProcessComponentFactory::~ProcessComponentFactory() { }
}
