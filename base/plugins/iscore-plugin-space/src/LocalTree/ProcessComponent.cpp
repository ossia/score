#include "ProcessComponent.hpp"

namespace Space
{
namespace LocalTree
{

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
}
}
