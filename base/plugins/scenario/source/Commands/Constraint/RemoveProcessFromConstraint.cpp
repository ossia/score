#include "RemoveProcessFromConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessSharedModelInterfaceSerialization.hpp"

#include <QDebug>

using namespace iscore;
using namespace Scenario::Command;

RemoveProcessFromConstraint::RemoveProcessFromConstraint(ObjectPath&& constraintPath, int processId):
	SerializableCommand{"ScenarioControl",
						"DeleteProcessFromConstraintCommand",
						"Delete process"},
	m_path{std::move(constraintPath)},
	m_processId{processId}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	Serializer<DataStream> s{&m_serializedProcessData};

	s.visit(static_cast<const ProcessSharedModelInterface&>(*constraint->process(m_processId)));
}

void RemoveProcessFromConstraint::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	Deserializer<DataStream> s{&m_serializedProcessData};
	constraint->addProcess(createProcess(s, constraint));
}

void RemoveProcessFromConstraint::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	constraint->removeProcess(m_processId);
}

int RemoveProcessFromConstraint::id() const
{
	return 1;
}

bool RemoveProcessFromConstraint::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveProcessFromConstraint::serializeImpl(QDataStream& s)
{
	s << m_path << m_processId << m_serializedProcessData;
}

void RemoveProcessFromConstraint::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_processId >> m_serializedProcessData;
}
