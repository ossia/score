#include "HideBoxInViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

HideBoxInViewModel::HideBoxInViewModel():
	SerializableCommand{"ScenarioControl",
						"HideBoxInViewModel",
						QObject::tr("Hide box in constraint view")}
{
}

HideBoxInViewModel::HideBoxInViewModel(ObjectPath&& path):
	SerializableCommand{"ScenarioControl",
						"HideBoxInViewModel",
						QObject::tr("Hide box in constraint view")},
	m_constraintViewModelPath{std::move(path)}
{
	auto constraint_vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
	m_constraintPreviousId = constraint_vm->shownBox();
}

HideBoxInViewModel::HideBoxInViewModel(AbstractConstraintViewModel* constraint_vm):
	SerializableCommand{"ScenarioControl",
						"HideBoxInViewModel",
						QObject::tr("Hide box in constraint view")},
	m_constraintViewModelPath{ObjectPath::pathFromObject("BaseConstraintModel",
														 constraint_vm)}
{
	m_constraintPreviousId = constraint_vm->shownBox();
}

void HideBoxInViewModel::undo()
{
	auto vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
	vm->showBox(m_constraintPreviousId);
}

void HideBoxInViewModel::redo()
{
	auto vm = m_constraintViewModelPath.find<AbstractConstraintViewModel>();
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
