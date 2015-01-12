#include "SetMaxDuration.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

SetMaxDuration::SetMaxDuration():
	SerializableCommand{"ScenarioControl",
						"SetMaxDuration",
						QObject::tr("Set max duration of constraint")}
{
}

SetMaxDuration::SetMaxDuration(ObjectPath&& constraintPath, int newDuration):
	SerializableCommand{"ScenarioControl",
						"SetMaxDuration",
						QObject::tr("Set max duration of constraint")},
	m_path{constraintPath},
	m_oldDuration{m_path.find<ConstraintModel>()->maxDuration()},
	m_newDuration{newDuration}
{
}

void SetMaxDuration::undo()
{
	auto constraint = m_path.find<ConstraintModel>();
	constraint->setMaxDuration(m_oldDuration);
}

void SetMaxDuration::redo()
{
	auto constraint = m_path.find<ConstraintModel>();
	constraint->setMaxDuration(m_newDuration);
}

int SetMaxDuration::id() const
{
	return 1;
}

bool SetMaxDuration::mergeWith(const QUndoCommand* other)
{
	return false;
}

void SetMaxDuration::serializeImpl(QDataStream& s)
{
	s << m_path << m_oldDuration << m_newDuration;
}

void SetMaxDuration::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_oldDuration >> m_newDuration;
}
