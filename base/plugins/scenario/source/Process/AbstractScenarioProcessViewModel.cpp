#include "AbstractScenarioProcessViewModel.hpp"
#include "Document/Constraint/ConstraintViewModelInterface.hpp"

ConstraintViewModelInterface* AbstractScenarioProcessViewModel::constraint(int constraintViewModelid) const
{
	return findById(m_constraints, constraintViewModelid);
}

QVector<ConstraintViewModelInterface*> AbstractScenarioProcessViewModel::constraints() const
{
	return m_constraints;
}


void AbstractScenarioProcessViewModel::removeConstraintViewModel(int constraintViewModelId)
{
	removeById(m_constraints, constraintViewModelId);
	emit constraintViewModelRemoved(constraintViewModelId);
}
