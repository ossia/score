#include "ClearConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"

#include <core/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

ClearConstraint::ClearConstraint():
	iscore::SerializableCommand{"ScenarioControl",
								"ClearConstraint",
								QObject::tr("Clear a box")}
{

}

ClearConstraint::ClearConstraint(ObjectPath&& constraintPath):
	iscore::SerializableCommand{"ScenarioControl",
								"ClearConstraint",
								QObject::tr("Clear a box")},
	m_path{std::move(constraintPath)}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	// Save the boxes
	serializeVectorOfPointers(constraint->boxes(),
							  m_serializedBoxes);

	// Save the processes
	for(const auto& process : constraint->processes())
	{
		QByteArray arr;

		QDataStream s(&arr, QIODevice::WriteOnly);
		s.setVersion(QDataStream::Qt_5_3);

		s << process->processName();
		s << *process;

		m_serializedProcesses.push_back(arr);
	}

	// TODO save the mapping in the parent scenario view models.
}

void ClearConstraint::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	for(auto& serializedProcess : m_serializedProcesses)
	{
		QDataStream s(&serializedProcess, QIODevice::ReadOnly);
		constraint->createProcess(s);
	}

	for(auto& serializedBox : m_serializedBoxes)
	{
		QDataStream s(&serializedBox, QIODevice::ReadOnly);
		constraint->createBox(s);
	}
}

void ClearConstraint::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	for(auto& process : constraint->processes())
	{
		constraint->removeProcess(process->id());
	}

	for(auto& box : constraint->boxes())
	{
		constraint->removeBox(box->id());
	}
}

int ClearConstraint::id() const
{
	return 1;
}

bool ClearConstraint::mergeWith(const QUndoCommand* other)
{
	return false;
}

void ClearConstraint::serializeImpl(QDataStream& s)
{
	s << m_path << m_serializedBoxes << m_serializedProcesses << m_scenarioViewModelsBoxMappings;
}

void ClearConstraint::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_serializedBoxes >> m_serializedProcesses >> m_scenarioViewModelsBoxMappings;
}
