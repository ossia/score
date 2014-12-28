#include "RemoveBoxFromConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"

#include "serialization/DataStreamVisitor.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveBoxFromConstraint::RemoveBoxFromConstraint(ObjectPath&& constraintPath, int boxId):
	SerializableCommand{"ScenarioControl",
						"RemoveBoxFromConstraint",
						"Remove box"},
	m_path{constraintPath},
	m_boxId{boxId}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	Serializer<DataStream> s{&m_serializedBoxData};
	s.readFrom(*constraint->box(boxId));
}

void RemoveBoxFromConstraint::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	Deserializer<DataStream> s{&m_serializedBoxData};
	constraint->addBox(new BoxModel{s, constraint});
}

void RemoveBoxFromConstraint::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
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

void RemoveBoxFromConstraint::serializeImpl(QDataStream& s)
{
	s << m_path << m_boxId << m_serializedBoxData;
}

void RemoveBoxFromConstraint::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_boxId >> m_serializedBoxData;
}
