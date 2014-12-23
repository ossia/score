#include "ShowBoxInViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;
// TODO ScenarioCommand which inherits from SerializableCommand and has ScenarioControl set
ShowBoxInViewModel::ShowBoxInViewModel(AbstractConstraintViewModel* constraint,
									   int boxId):
	SerializableCommand{"ScenarioControl",
						"CreateEventCommand",
						QObject::tr("Show box in constraint view")},
	m_constraintViewModelPath{ObjectPath::pathFromObject("BaseConstraintModel",
														 constraint)},
	m_boxId{boxId}
{

	m_constraintPreviousState = constraint->isBoxShown();
	m_constraintPreviousId = constraint->shownBox();
}

void ShowBoxInViewModel::undo()
{
	auto vm = static_cast<AbstractConstraintViewModel*>(m_constraintViewModelPath.find());
	if(m_constraintPreviousState)
	{
		vm->showBox(m_constraintPreviousId);
	}
	else
	{
		vm->hideBox();
	}
}

void ShowBoxInViewModel::redo()
{
	auto vm = static_cast<AbstractConstraintViewModel*>(m_constraintViewModelPath.find());
	vm->showBox(m_boxId);
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
