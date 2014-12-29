#include "ShowBoxInViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

// TODO ScenarioCommand which inherits from SerializableCommand and has ScenarioControl set
ShowBoxInViewModel::ShowBoxInViewModel():
	SerializableCommand{"ScenarioControl",
						"ShowBoxInViewModel",
						QObject::tr("Show box in constraint view")}
{
}

ShowBoxInViewModel::ShowBoxInViewModel(ObjectPath&& constraint_path,
									   int boxId):
	SerializableCommand{"ScenarioControl",
						"ShowBoxInViewModel",
						QObject::tr("Show box in constraint view")},
	m_constraintViewModelPath{std::move(constraint_path)},
	m_boxId{boxId}
{
	auto constraint_vm = static_cast<AbstractConstraintViewModel*>(m_constraintViewModelPath.find());
	m_constraintPreviousState = constraint_vm->isBoxShown();
	m_constraintPreviousId = constraint_vm->shownBox();
}

ShowBoxInViewModel::ShowBoxInViewModel(AbstractConstraintViewModel* constraint_vm,
									   int boxId):
	SerializableCommand{"ScenarioControl",
						"ShowBoxInViewModel",
						QObject::tr("Show box in constraint view")},
	m_constraintViewModelPath{ObjectPath::pathFromObject("BaseConstraintModel",
														 constraint_vm)},
	m_boxId{boxId}
{

	m_constraintPreviousState = constraint_vm->isBoxShown();
	m_constraintPreviousId = constraint_vm->shownBox();
}

void ShowBoxInViewModel::undo()
{
	auto constraint_vm = static_cast<AbstractConstraintViewModel*>(m_constraintViewModelPath.find());
	if(m_constraintPreviousState)
	{
		constraint_vm->showBox(m_constraintPreviousId);
	}
	else
	{
		constraint_vm->hideBox();
	}
}

void ShowBoxInViewModel::redo()
{
	auto constraint_vm = static_cast<AbstractConstraintViewModel*>(m_constraintViewModelPath.find());
	constraint_vm->showBox(m_boxId);
}

int ShowBoxInViewModel::id() const
{
	return 1;
}

bool ShowBoxInViewModel::mergeWith(const QUndoCommand* other)
{
	return false;
}

void ShowBoxInViewModel::serializeImpl(QDataStream& s)
{
	s << m_constraintViewModelPath
	  << m_boxId
	  << m_constraintPreviousId
	  << m_constraintPreviousState;
}

void ShowBoxInViewModel::deserializeImpl(QDataStream& s)
{
	s >> m_constraintViewModelPath
	  >> m_boxId
	  >> m_constraintPreviousId
	  >> m_constraintPreviousState;
}
