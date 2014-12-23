#include "BaseElementModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"
#include <iostream>

BaseElementModel::BaseElementModel(QObject* parent):
	iscore::DocumentDelegateModelInterface{"BaseElementModel", parent},
	m_baseConstraint{new ConstraintModel{0, this}},
	m_viewModel{static_cast<TemporalConstraintViewModel*>(m_baseConstraint->makeViewModel("Temporal", 0, this))}
{
	m_baseConstraint->setWidth(1000);
	m_baseConstraint->setHeight(1000);
	m_baseConstraint->setObjectName("BaseConstraintModel");
}

void BaseElementModel::reset()
{
}

