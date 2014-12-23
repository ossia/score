#include "HideBoxInViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;
// TODO ScenarioCommand which inherits from SerializableCommand and has ScenarioControl set
HideBoxInViewModel::HideBoxInViewModel(AbstractConstraintViewModel* constraint):
	SerializableCommand{"ScenarioControl",
						"HideBoxInViewModel",
						QObject::tr("Hide box in constraint view")},
	m_constraintViewModelPath{ObjectPath::pathFromObject("BaseConstraintModel",
														 constraint)}
{
	m_constraintPreviousId = constraint->shownBox();
}

void HideBoxInViewModel::undo()
{
	auto vm = static_cast<AbstractConstraintViewModel*>(m_constraintViewModelPath.find());
	vm->showBox(m_constraintPreviousId);
}

void HideBoxInViewModel::redo()
{
	auto vm = static_cast<AbstractConstraintViewModel*>(m_constraintViewModelPath.find());
	vm->hideBox();
}

int HideBoxInViewModel::id() const
{
	return 1;
}

bool HideBoxInViewModel::mergeWith(const QUndoCommand* other)
{
	return false;
}

void HideBoxInViewModel::serializeImpl(QDataStream& s)
{
	s << m_constraintViewModelPath
	  << m_constraintPreviousId;
}

void HideBoxInViewModel::deserializeImpl(QDataStream& s)
{
	s >> m_constraintViewModelPath
	  >> m_constraintPreviousId;
}
