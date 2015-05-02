#include "RemoveBoxFromConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveBoxFromConstraint::RemoveBoxFromConstraint(ObjectPath&& boxPath) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()}
{
    auto constraintPath = boxPath.vec();
    auto lastId = constraintPath.takeLast();
    m_path = ObjectPath{std::move(constraintPath) };
    m_boxId = id_type<BoxModel> (lastId.id());

    auto& constraint = m_path.find<ConstraintModel>();
    // Save the box
    Serializer<DataStream> s{&m_serializedBoxData};
    s.readFrom(*constraint.box(m_boxId));

    // Save for each view model of this constraint
    // a bool indicating if the box being deleted
    // was displayed
    for(const AbstractConstraintViewModel* vm : constraint.viewModels())
    {
        m_boxMappings[vm->id()] = vm->shownBox() == m_boxId;
    }
}

RemoveBoxFromConstraint::RemoveBoxFromConstraint(
        ObjectPath&& constraintPath,
        id_type<BoxModel> boxId) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {constraintPath},
    m_boxId {boxId}
{
    auto& constraint = m_path.find<ConstraintModel>();

    Serializer<DataStream> s{&m_serializedBoxData};
    s.readFrom(*constraint.box(m_boxId));

    for(const AbstractConstraintViewModel* vm : constraint.viewModels())
    {
        m_boxMappings[vm->id()] = vm->shownBox() == m_boxId;
    }
}

void RemoveBoxFromConstraint::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    Deserializer<DataStream> s {m_serializedBoxData};
    constraint.addBox(new BoxModel {s, &constraint});

    for(AbstractConstraintViewModel* vm : constraint.viewModels())
    {
        if(m_boxMappings[vm->id()])
        {
            vm->showBox(m_boxId);
        }
    }
}

void RemoveBoxFromConstraint::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    constraint.removeBox(m_boxId);
}

void RemoveBoxFromConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_boxId << m_serializedBoxData << m_boxMappings;
}

void RemoveBoxFromConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_boxId >> m_serializedBoxData >> m_boxMappings;
}
