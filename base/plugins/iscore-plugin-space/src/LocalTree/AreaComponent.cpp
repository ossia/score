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
    m_computations{add_node(*node(), "computations")},
    m_hm{*this, process, doc, ctx, this}
{
    Ossia::LocalTree::make_metadata_node(process.metadata, *node(), m_properties, this);
}

template<>
AreaComponent* ProcessLocalTree::make<AreaComponent, AreaModel, AreaComponentFactory>(
        const Id<Component>& id,
        AreaComponentFactory& fact,
        AreaModel& elt,
        const system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return fact.make(id, *m_areas, elt, doc, ctx, parent);
}

template<>
ComputationComponent* ProcessLocalTree::make<ComputationComponent, ComputationModel, ComputationComponentFactory>(
        const Id<Component>& id,
        ComputationComponentFactory& fact,
        ComputationModel& elt,
        const system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent)
{
    return fact.make(id, *m_computations, elt, doc, ctx, parent);
}

void ProcessLocalTree::removing(const AreaModel& elt, const AreaComponent& comp)
{
    auto it = find_if(m_areas->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_areas->children().end());

    m_areas->eraseAndNotify(it);
}

void ProcessLocalTree::removing(const ComputationModel& elt, const ComputationComponent& comp)
{
    auto it = find_if(m_computations->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_computations->children().end());

    m_areas->eraseAndNotify(it);
}

AreaComponentFactory::~AreaComponentFactory()
{

}

AreaComponent::AreaComponent(
        OSSIA::Node& node,
        AreaModel& area,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component{id, name, parent},
    m_thisNode{node, area.metadata, this}
{
}

AreaComponent::~AreaComponent()
{

}

const std::shared_ptr<OSSIA::Node>& AreaComponent::node() const
{ return m_thisNode.node; }

OSSIA::Node& AreaComponent::thisNode() const
{ return *node(); }

GenericAreaComponent::GenericAreaComponent(
        const Id<iscore::Component>& cmp,
        OSSIA::Node& parent_node,
        AreaModel& area,
        const Ossia::LocalTree::DocumentPlugin& doc,
        const iscore::DocumentContext& ctx,
        QObject* paren_objt):
    AreaComponent{parent_node, area, cmp, "GenericAreaComponent", paren_objt}
{
    Ossia::LocalTree::make_metadata_node(area.metadata, *node(), m_properties, this);
}

AreaComponent* GenericAreaComponentFactory::make(
        const Id<iscore::Component>& cmp,
        OSSIA::Node& parent,
        AreaModel& proc,
        const Ossia::LocalTree::DocumentPlugin& doc,
        const iscore::DocumentContext& ctx,
        QObject* paren_objt) const
{
    return new GenericAreaComponent{
        cmp, parent, proc, doc, ctx, paren_objt};
}

const GenericAreaComponentFactory::factory_key_type& GenericAreaComponentFactory::key_impl() const
{
    static const factory_key_type name{"AreaComponentFactory"};
    return name;
}

bool GenericAreaComponentFactory::matches(
        AreaModel& p,
        const Ossia::LocalTree::DocumentPlugin&,
        const iscore::DocumentContext&) const
{
    return true;
}





ComputationComponentFactory::~ComputationComponentFactory()
{

}

ComputationComponent::ComputationComponent(
        OSSIA::Node& node,
        ComputationModel& computation,
        const Id<iscore::Component>& id,
        const QString& name,
        QObject* parent):
    Component{id, name, parent},
    m_thisNode{node, computation.metadata, this}
{
}

ComputationComponent::~ComputationComponent()
{

}

const std::shared_ptr<OSSIA::Node>& ComputationComponent::node() const
{ return m_thisNode.node; }

OSSIA::Node& ComputationComponent::thisNode() const
{ return *node(); }




}


}



ISCORE_METADATA_IMPL(Space::LocalTree::AreaComponent)
ISCORE_METADATA_IMPL(Space::LocalTree::ComputationComponent)
