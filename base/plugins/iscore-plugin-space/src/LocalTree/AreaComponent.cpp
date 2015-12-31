#include "AreaComponent.hpp"

namespace Space
{

namespace LocalTree
{

ProcessLocalTreeFactory::~ProcessLocalTreeFactory()
{

}

const ProcessLocalTreeFactory::factory_key_type&
ProcessLocalTreeFactory::key_impl() const
{
    static const factory_key_type name{"SpaceComponentFactory"};
    return name;

}

bool ProcessLocalTreeFactory::matches(
        Process::ProcessModel& p,
        const Ossia::LocalTree::DocumentPlugin&,
        const iscore::DocumentContext&) const
{
    return dynamic_cast<Space::ProcessModel*>(&p);
}


Ossia::LocalTree::ProcessComponent* ProcessLocalTreeFactory::make(
        const Id<iscore::Component>& id,
        OSSIA::Node& parent,
        Process::ProcessModel& proc,
        const Ossia::LocalTree::DocumentPlugin& doc,
        const iscore::DocumentContext& ctx,
        QObject* paren_objt) const
{
    return new ProcessLocalTree{
        id, parent,
                static_cast<Space::ProcessModel&>(proc),
                doc, ctx, paren_objt};
}

ProcessLocalTree::ProcessLocalTree(
        const Id<iscore::Component>& id,
        OSSIA::Node& parent,
        ProcessModel& process,
        const ProcessLocalTree::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_obj):
    ProcessComponent{parent, process, id, "SpaceComponent", parent_obj},
    m_areas{add_node(*node(), "areas")},
    m_computations{add_node(*node(), "computations")}
{
    Ossia::LocalTree::make_metadata_node(process.metadata, *node(), m_properties, this);
    for(auto& area : process.areas)
    {

    }

    for(auto& component : process.computations)
    {

    }
}

AreaComponentFactory::~AreaComponentFactory()
{

}

AreaComponent::~AreaComponent()
{

}
}
}



ISCORE_METADATA_IMPL(Space::LocalTree::AreaComponent)
