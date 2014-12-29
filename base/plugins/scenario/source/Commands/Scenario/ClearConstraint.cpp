#include "ClearConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Process/Temporal/TemporalScenarioProcessViewModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterfaceSerialization.hpp"

#include <core/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

ClearConstraint::ClearConstraint():
	SerializableCommand{"ScenarioControl",
								"ClearConstraint",
								QObject::tr("Clear a box")}
{

}

ClearConstraint::ClearConstraint(ObjectPath&& constraintPath):
	SerializableCommand{"ScenarioControl",
								"ClearConstraint",
								QObject::tr("Clear a box")},
	m_path{std::move(constraintPath)}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	for(const BoxModel* box : constraint->boxes())
	{
		QByteArray arr;
		Serializer<DataStream> s{&arr};
		s.readFrom(*box);
		m_serializedBoxes.push_back(arr);
	}

	for(const ProcessSharedModelInterface* process : constraint->processes())
	{
		QByteArray arr;
		Serializer<DataStream> s{&arr};
		s.readFrom(*process);
		m_serializedProcesses.push_back(arr);
	}

	// @todo save the mapping in the parent scenario view models.
}

void ClearConstraint::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	for(auto& serializedProcess : m_serializedProcesses)
	{
		Deserializer<DataStream> s{&serializedProcess};
		constraint->addProcess(createProcess(s, constraint));
	}

	for(auto& serializedBox : m_serializedBoxes)
	{
		Deserializer<DataStream> s{&serializedBox};
		constraint->addBox(new BoxModel{s, constraint});
	}


}

void ClearConstraint::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	for(auto& process : constraint->processes())
	{
		constraint->removeProcess((SettableIdentifier::identifier_type)process->id());
	}

	for(auto& box : constraint->boxes())
	{
		constraint->removeBox((SettableIdentifier::identifier_type)box->id());
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
