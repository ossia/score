#include "Process.hpp"

#include <Process/Process.hpp>

RemoteControl::ProcessComponent::ProcessComponent(
    Process::ProcessModel& process, DocumentPlugin& doc, const QString& name,
    QObject* parent)
    : Process::GenericProcessComponent<DocumentPlugin>{process, doc, name, parent}
{
}

RemoteControl::ProcessComponent::~ProcessComponent() { }

RemoteControl::ProcessComponentFactory::~ProcessComponentFactory() { }
