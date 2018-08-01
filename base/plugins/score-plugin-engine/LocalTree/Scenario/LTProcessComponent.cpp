// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessComponent.hpp"



#include <Process/Process.hpp>

Engine::LocalTree::ProcessComponent::ProcessComponent(
    ossia::net::node_base& parentNode,
    Process::ProcessModel& proc,
    DocumentPlugin& doc,
    const Id<score::Component>& id,
    const QString& name,
    QObject* parent)
    : Component<Process::GenericProcessComponent<DocumentPlugin>>{
          parentNode, proc.metadata(), proc, doc, id, name, parent}
{
}

Engine::LocalTree::ProcessComponent::~ProcessComponent() = default;
Engine::LocalTree::ProcessComponentFactory::~ProcessComponentFactory()
    = default;
Engine::LocalTree::ProcessComponentFactoryList::~ProcessComponentFactoryList()
    = default;
