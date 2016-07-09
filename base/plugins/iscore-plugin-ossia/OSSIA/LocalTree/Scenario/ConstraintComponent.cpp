#include "ConstraintComponent.hpp"
#include <iscore/tools/std/Algorithms.hpp>
#include "MetadataParameters.hpp"

namespace Ossia
{
namespace LocalTree
{
ConstraintBase::ConstraintBase(
        OSSIA::Node& parent,
        const Id<iscore::Component>& id,
        Scenario::ConstraintModel& constraint,
        ConstraintBase::system_t& doc,
        QObject* parent_comp):
    parent_t{constraint, doc, id, "ConstraintComponent", parent_comp},
    m_thisNode{parent, constraint.metadata, this},
    m_processesNode{add_node(thisNode(), "processes")}
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

ConstraintBase::~ConstraintBase()
{
    m_properties.clear();

    m_processesNode.reset();
    m_thisNode.clear();
}


ProcessComponent*ConstraintBase::make_processComponent(
        const Id<iscore::Component>& id,
        ProcessComponentFactory& factory,
        Process::ProcessModel& process)
{
    return factory.make(id, *m_processesNode, process, system(), this);
}


void ConstraintBase::removing(
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
