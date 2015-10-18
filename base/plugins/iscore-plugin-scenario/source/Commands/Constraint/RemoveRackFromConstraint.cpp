#include "RemoveRackFromConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"

#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveRackFromConstraint::RemoveRackFromConstraint(
        Path<RackModel>&& rackPath) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()}
{
    auto constraintPath = rackPath.unsafePath().vec();
    auto lastId = constraintPath.takeLast();
    m_path = Path<ConstraintModel>{ObjectPath{std::move(constraintPath)},
              Path<ConstraintModel>::UnsafeDynamicCreation{}};
    m_rackId = Id<RackModel> (lastId.id());

    auto& constraint = m_path.find();
    // Save the rack
    Serializer<DataStream> s{&m_serializedRackData};
    s.readFrom(constraint.racks.at(m_rackId));

    // Save for each view model of this constraint
    // a bool indicating if the rack being deleted
    // was displayed
    for(const ConstraintViewModel* vm : constraint.viewModels())
    {
        m_rackMappings[vm->id()] = vm->shownRack() == m_rackId;
    }
}

RemoveRackFromConstraint::RemoveRackFromConstraint(
        Path<ConstraintModel>&& constraintPath,
        Id<RackModel> rackId) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_path {constraintPath},
    m_rackId {rackId}
{
    auto& constraint = m_path.find();

    Serializer<DataStream> s{&m_serializedRackData};
    s.readFrom(constraint.racks.at(m_rackId));

    for(const ConstraintViewModel* vm : constraint.viewModels())
    {
        m_rackMappings[vm->id()] = vm->shownRack() == m_rackId;
    }
}

void RemoveRackFromConstraint::undo() const
{
    auto& constraint = m_path.find();
    Deserializer<DataStream> s {m_serializedRackData};
    constraint.racks.add(new RackModel {s, &constraint});

    for(ConstraintViewModel* vm : constraint.viewModels())
    {
        if(m_rackMappings[vm->id()])
        {
            vm->showRack(m_rackId);
        }
    }
}

void RemoveRackFromConstraint::redo() const
{
    auto& constraint = m_path.find();
    constraint.racks.remove(m_rackId);
}

void RemoveRackFromConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_rackId << m_serializedRackData << m_rackMappings;
}

void RemoveRackFromConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_rackId >> m_serializedRackData >> m_rackMappings;
}
