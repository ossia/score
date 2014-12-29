#include "ShowBoxInViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

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
	auto constraint_vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
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
	auto constraint_vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
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
	auto constraint_vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
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
