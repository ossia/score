#include "ConstraintComponent.hpp"
#include <iscore/tools/std/Algorithms.hpp>
#include "MetadataParameters.hpp"

namespace OSSIA
{
namespace LocalTree
{


const iscore::Component::Key&ConstraintComponent::key() const
{
    static const Key k{"OSSIA::LocalTree::ConstraintComponent"};
    return k;
}


ConstraintComponent::ConstraintComponent(
        Node& parent,
        const Id<iscore::Component>& id,
        ConstraintModel& constraint,
        const ConstraintComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_comp):
    Component{id, "ConstraintComponent", parent_comp},
    m_thisNode{parent, constraint.metadata, this},
    m_processesNode{add_node(thisNode(), "processes")},
    m_baseComponent{*this, constraint, doc, ctx, this}
{
    using tv_t = ::TimeValue;
    make_metadata_node(constraint.metadata, thisNode(), m_properties, this);

    m_properties.push_back(
                add_property<float>(thisNode(), "yPos", &constraint,
                        &ConstraintModel::heightPercentage,
                        &ConstraintModel::setHeightPercentage,
                        &ConstraintModel::heightPercentageChanged,
                        this));

    m_properties.push_back(
    add_getProperty<tv_t>(thisNode(), "min", &constraint.duration,
                          &ConstraintDurations::minDuration,
                          &ConstraintDurations::minDurationChanged,
                          this));

    m_properties.push_back(
    add_getProperty<tv_t>(thisNode(), "max", &constraint.duration,
                          &ConstraintDurations::maxDuration,
                          &ConstraintDurations::maxDurationChanged,
                          this));

    m_properties.push_back(
    add_getProperty<tv_t>(thisNode(), "default", &constraint.duration,
                          &ConstraintDurations::defaultDuration,
                          &ConstraintDurations::defaultDurationChanged,
                          this));

    m_properties.push_back(
    add_getProperty<float>(thisNode(), "play", &constraint.duration,
                           &ConstraintDurations::playPercentage,
                           &ConstraintDurations::playPercentageChanged,
                           this));
}

ConstraintComponent::~ConstraintComponent()
{
    for(auto prop : m_properties)
        delete prop;
}


ProcessComponent*ConstraintComponent::make_processComponent(const Id<iscore::Component>& id, ProcessComponentFactory& factory, Process& process, const DocumentPlugin& system, const iscore::DocumentContext& ctx, QObject* parent_component)
{
    return factory.make(id, *m_processesNode, process, system, ctx, parent_component);
}


void ConstraintComponent::removing(const Process& cst, const ProcessComponent& comp)
{
    auto it = find_if(m_processesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_processesNode->children().end());

    m_processesNode->eraseAndNotify(it);
}


}
}
