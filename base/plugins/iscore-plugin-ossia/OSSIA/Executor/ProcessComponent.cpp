#include "ProcessElement.hpp"

#include <OSSIA/Executor/DocumentPlugin.hpp>
ISCORE_METADATA_IMPL(RecreateOnPlay::ProcessComponent)
namespace RecreateOnPlay
{

ProcessComponent::ProcessComponent(
            ConstraintElement &cst,
            Process::ProcessModel &proc,
            const Id<iscore::Component> &id,
            const QString &name,
            QObject *parent):
    iscore::Component{id, name, parent},
    m_parent_constraint{cst},
    m_iscore_process{proc}
{

}

ProcessComponent::~ProcessComponent()
{

}

ProcessComponentFactory::~ProcessComponentFactory()
{

}

}
