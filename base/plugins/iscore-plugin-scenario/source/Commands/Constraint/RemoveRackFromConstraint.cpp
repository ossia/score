#include "RemoveRackFromConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"

#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveRackFromConstraint::RemoveRackFromConstraint(ObjectPath&& rackPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()}
{
    auto constraintPath = rackPath.vec();
    auto lastId = constraintPath.takeLast();
    m_path = ObjectPath{std::move(constraintPath) };
    m_rackId = id_type<RackModel> (lastId.id());

    auto& constraint = m_path.find<ConstraintModel>();
    // Save the rack
    Serializer<DataStream> s{&m_serializedRackData};
    s.readFrom(constraint.rack(m_rackId));

    // Save for each view model of this constraint
    // a bool indicating if the rack being deleted
    // was displayed
    for(const ConstraintViewModel* vm : constraint.viewModels())
    {
        m_rackMappings[vm->id()] = vm->shownRack() == m_rackId;
    }
}

RemoveRackFromConstraint::RemoveRackFromConstraint(
        ObjectPath&& constraintPath,
        id_type<RackModel> rackId) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {constraintPath},
    m_rackId {rackId}
{
    auto& constraint = m_path.find<ConstraintModel>();

    Serializer<DataStream> s{&m_serializedRackData};
    s.readFrom(constraint.rack(m_rackId));

    for(const ConstraintViewModel* vm : constraint.viewModels())
    {
        m_rackMappings[vm->id()] = vm->shownRack() == m_rackId;
    }
}

void RemoveRackFromConstraint::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    Deserializer<DataStream> s {m_serializedRackData};
    constraint.addRack(new RackModel {s, &constraint});

    for(ConstraintViewModel* vm : constraint.viewModels())
    {
        if(m_rackMappings[vm->id()])
        {
            vm->showRack(m_rackId);
        }
    }
}

void RemoveRackFromConstraint::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    constraint.removeRack(m_rackId);
}

void RemoveRackFromConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_rackId << m_serializedRackData << m_rackMappings;
}

void RemoveRackFromConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_rackId >> m_serializedRackData >> m_rackMappings;
}
