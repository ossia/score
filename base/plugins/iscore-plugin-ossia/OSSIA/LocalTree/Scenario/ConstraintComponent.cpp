#include "ConstraintComponent.hpp"
#include <iscore/tools/std/Algorithms.hpp>
#include "MetadataParameters.hpp"

namespace Ossia
{
namespace LocalTree
{


const iscore::Component::Key&ConstraintComponent::key() const
{
    static const Key k{"Ossia::LocalTree::ConstraintComponent"};
    return k;
}


ConstraintComponent::ConstraintComponent(
        OSSIA::Node& parent,
        const Id<iscore::Component>& id,
        Scenario::ConstraintModel& constraint,
        ConstraintComponent::system_t& doc,
        const iscore::DocumentContext& ctx,
        QObject* parent_comp):
    Component{id, "ConstraintComponent", parent_comp},
    m_thisNode{parent, constraint.metadata, this},
    m_processesNode{add_node(thisNode(), "processes")},
    m_baseComponent{*this, constraint, doc, ctx, this}
{
    using namespace Scenario;
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

    m_properties.push_back(
      add_property<double>(thisNode(), "speed", &constraint.duration,
                           &ConstraintDurations::executionSpeed,
                           &ConstraintDurations::setExecutionSpeed,
                           &ConstraintDurations::executionSpeedChanged,
                           this));
}

ConstraintComponent::~ConstraintComponent()
{
}


ProcessComponent*ConstraintComponent::make_processComponent(
        const Id<iscore::Component>& id,
        ProcessComponentFactory& factory,
        Process::ProcessModel& process,
        DocumentPlugin& system,
        const iscore::DocumentContext& ctx,
        QObject* parent_component)
{
    return factory.make(id, *m_processesNode, process, system, ctx, parent_component);
}


void ConstraintComponent::removing(
        const Process::ProcessModel& cst,
        const ProcessComponent& comp)
{
    auto it = find_if(m_processesNode->children(), [&] (const auto& node)
    { return node == comp.node(); });
    ISCORE_ASSERT(it != m_processesNode->children().end());

    m_processesNode->erase(it);
}


}
}
