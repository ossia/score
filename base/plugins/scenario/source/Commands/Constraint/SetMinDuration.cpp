#include "SetMinDuration.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

SetMinDuration::SetMinDuration():
	SerializableCommand{"ScenarioControl",
						"SetMinDuration",
						QObject::tr("Set min duration of constraint")}
{
}

SetMinDuration::SetMinDuration(ObjectPath&& constraintPath, int newDuration):
	SerializableCommand{"ScenarioControl",
						"SetMinDuration",
						QObject::tr("Set min duration of constraint")},
	m_path{constraintPath},
	m_oldDuration{m_path.find<ConstraintModel>()->maxDuration()},
	m_newDuration{newDuration}
{
}

void SetMinDuration::undo()
{
	auto constraint = m_path.find<ConstraintModel>();
	constraint->setMinDuration(m_oldDuration);
}

void SetMinDuration::redo()
{
	auto constraint = m_path.find<ConstraintModel>();
	constraint->setMinDuration(m_newDuration);
}

int SetMinDuration::id() const
{
	return 1;
}

bool SetMinDuration::mergeWith(const QUndoCommand* other)
{
	return false;
}

void SetMinDuration::serializeImpl(QDataStream& s)
{
	s << m_path << m_oldDuration << m_newDuration;
}

void SetMinDuration::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_oldDuration >> m_newDuration;
}
