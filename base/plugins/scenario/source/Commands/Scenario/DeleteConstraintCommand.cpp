#include "DeleteConstraintCommand.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Process/ScenarioProcessViewModel.hpp"

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

		s << *process;

		m_serializedProcesses.push_back(arr);
	}

	// Save the mappings in the scenario view models
	// in which this constraint is displayed
	// For each SVM corresponding to the shared model this constraint is in,
	// save a tuple {box_id, deck_id, pvm_id},
	// and the associated box id for the current constraint.

	// This, of course, cannot be done for the Base constraint,
	// since it is not in any scenarioviewmodel.
	// Instead, you have to create a New Document.

	if(auto scenarioSharedModel = dynamic_cast<ScenarioProcessSharedModel*>(constraint->parent()))
	{
		for(QObject* pvm_uncasted : scenarioSharedModel->viewModels())
		{
			auto pvm = static_cast<ScenarioProcessViewModel*>(pvm_uncasted);
			auto deckModel = static_cast<DeckModel*>(pvm->parent());
			auto boxModel = static_cast<BoxModel*>(deckModel->parent());

			m_scenarioViewModelsBoxMappings[
				std::tuple<int,int,int>{
					boxModel->id(),
					deckModel->id(),
					pvm->id()}
			] = pvm->boxDisplayedForConstraint(constraint->id());
		}
	}
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

	if(auto scenarioSharedModel = dynamic_cast<ScenarioProcessSharedModel*>(constraint->parent()))
	{
		for(QObject* pvm_uncasted : scenarioSharedModel->viewModels())
		{
			auto pvm = static_cast<ScenarioProcessViewModel*>(pvm_uncasted);
			auto deckModel = static_cast<DeckModel*>(pvm->parent());
			auto boxModel = static_cast<BoxModel*>(deckModel->parent());
			std::tuple<int,int,int> tpl{boxModel->id(),
										deckModel->id(),
										pvm->id()};

			if(m_scenarioViewModelsBoxMappings.contains(tpl))
			{
				pvm->boxDisplayedForConstraint(constraint->id()) = m_scenarioViewModelsBoxMappings[tpl];
			}
		}
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

	if(auto scenarioSharedModel = dynamic_cast<ScenarioProcessSharedModel*>(constraint->parent()))
	{
		for(QObject* pvm_uncasted : scenarioSharedModel->viewModels())
		{
			auto pvm = static_cast<ScenarioProcessViewModel*>(pvm_uncasted);
			pvm->boxDisplayedForConstraint(constraint->id()) = {false, {}};
		}
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


// TODO
void ClearConstraint::serializeImpl(QDataStream& s)
{
	s << m_path << m_serializedBoxes << m_serializedProcesses << m_scenarioViewModelsBoxMappings;
}

void ClearConstraint::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_serializedBoxes >> m_serializedProcesses >> m_scenarioViewModelsBoxMappings;
}
