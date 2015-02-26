#include "RemoveBoxFromConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "serialization/DataStreamVisitor.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveBoxFromConstraint::RemoveBoxFromConstraint() :
    SerializableCommand {"ScenarioControl",
    "RemoveBoxFromConstraint",
    QObject::tr("Remove box")
}
{
}

RemoveBoxFromConstraint::RemoveBoxFromConstraint(ObjectPath&& boxPath) :
    SerializableCommand {"ScenarioControl",
    "RemoveBoxFromConstraint",
    QObject::tr("Remove box")
}
{
    auto constraintPath = boxPath.vec();
    auto lastId = constraintPath.takeLast();
    m_path = ObjectPath{std::move(constraintPath) };
    m_boxId = id_type<BoxModel> (lastId.id());
}

RemoveBoxFromConstraint::RemoveBoxFromConstraint(ObjectPath&& constraintPath, id_type<BoxModel> boxId) :
    SerializableCommand {"ScenarioControl",
    "RemoveBoxFromConstraint",
    QObject::tr("Remove box")
},
m_path {constraintPath},
m_boxId {boxId}
{
    auto constraint = m_path.find<ConstraintModel>();

    Serializer<DataStream> s{&m_serializedBoxData};
    s.readFrom(*constraint->box(boxId));
}

void RemoveBoxFromConstraint::undo()
{
    auto constraint = m_path.find<ConstraintModel>();
    Deserializer<DataStream> s {&m_serializedBoxData};
    constraint->addBox(new BoxModel {s, constraint});
}

void RemoveBoxFromConstraint::redo()
{
    auto constraint = m_path.find<ConstraintModel>();
    constraint->removeBox(m_boxId);
}

int RemoveBoxFromConstraint::id() const
{
    return 1;
}

bool RemoveBoxFromConstraint::mergeWith(const QUndoCommand* other)
{
    return false;
}

void RemoveBoxFromConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_boxId << m_serializedBoxData;
}

void RemoveBoxFromConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_boxId >> m_serializedBoxData;
}
