#include "ConstraintComponent.hpp"
#include <iscore/tools/std/Algorithms.hpp>

namespace Ossia
{
namespace LocalTree
{
ConstraintBase::ConstraintBase(
        OSSIA::Node& parent,
        const Id<iscore::Component>& id,
        Scenario::ConstraintModel& constraint,
        DocumentPlugin& doc,
        QObject* parent_comp):
    parent_t{parent, constraint.metadata, constraint, doc, id, "ConstraintComponent", parent_comp},
    m_processesNode{add_node(*node(), "processes")}
{
    using namespace Scenario;
    using tv_t = ::TimeValue;

    m_properties.push_back(
                add_property<float>(*node(), "yPos", &constraint,
                        &ConstraintModel::heightPercentage,
                        &ConstraintModel::setHeightPercentage,
                        &ConstraintModel::heightPercentageChanged,
                        this));

    m_properties.push_back(
    add_getProperty<tv_t>(*node(), "min", &constraint.duration,
                          &ConstraintDurations::minDuration,
                          &ConstraintDurations::minDurationChanged,
                          this));

    m_properties.push_back(
    add_getProperty<tv_t>(*node(), "max", &constraint.duration,
                          &ConstraintDurations::maxDuration,
                          &ConstraintDurations::maxDurationChanged,
                          this));

    m_properties.push_back(
    add_getProperty<tv_t>(*node(), "default", &constraint.duration,
                          &ConstraintDurations::defaultDuration,
                          &ConstraintDurations::defaultDurationChanged,
                          this));

    m_properties.push_back(
    add_getProperty<float>(*node(), "play", &constraint.duration,
                           &ConstraintDurations::playPercentage,
                           &ConstraintDurations::playPercentageChanged,
                           this));

    m_properties.push_back(
      add_property<double>(*node(), "speed", &constraint.duration,
                           &ConstraintDurations::executionSpeed,
                           &ConstraintDurations::setExecutionSpeed,
                           &ConstraintDurations::executionSpeedChanged,
                           this));
}

ProcessComponent* ConstraintBase::make(
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
