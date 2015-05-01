#include "RemoveConstraint.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Algorithms/StandardRemovalPolicy.hpp"

#include "Document/Constraint/ViewModels/AbstractConstraintViewModelSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveConstraint::RemoveConstraint(const ObjectPath &scenarioPath, ConstraintModel *constraint):
    RemoveConstraint{ObjectPath{scenarioPath}, constraint}
{

}

RemoveConstraint::RemoveConstraint(ObjectPath&& scenarioPath, ConstraintModel* constraint) :
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
m_path {std::move(scenarioPath) }
{
    Serializer<DataStream> cstReader{&m_serializedConstraint};
    cstReader.readFrom(*constraint);

    m_cstrId = constraint->id();

    auto scenar = m_path.find<ScenarioModel>();
    // We have to backup all the view models pointing to a constraint.
    // The full view is already back-upped by the serialization process.

    m_serializedConstraintViewModels = serializeConstraintViewModels(constraint, scenar);

}

void RemoveConstraint::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    Deserializer<DataStream> s {m_serializedConstraint};

    auto newConstraint = new ConstraintModel(s, scenar);
    scenar->addConstraint(newConstraint);

    scenar->event(newConstraint->startEvent())->addNextConstraint(newConstraint->id());
    scenar->event(newConstraint->endEvent())->addPreviousConstraint(newConstraint->id());

    deserializeConstraintViewModels(m_serializedConstraintViewModels, scenar);
}

void RemoveConstraint::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    StandardRemovalPolicy::removeConstraint(*scenar, m_cstrId);
}

void RemoveConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_cstrId << m_serializedConstraint << m_serializedConstraintViewModels << m_constraintFullViewId;
}

void RemoveConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_cstrId >> m_serializedConstraint >> m_serializedConstraintViewModels >> m_constraintFullViewId;
}
