#include "BaseElementModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include <iostream>

BaseElementModel::BaseElementModel(QObject* parent):
	iscore::DocumentDelegateModelInterface{"BaseElementModel", parent},
	m_baseConstraint{new ConstraintModel{0, this}}
{
	m_baseConstraint->m_width = 1000;
	m_baseConstraint->m_height = 1000;
	m_baseConstraint->setObjectName("BaseConstraintModel");
}

void BaseElementModel::reset()
{
}

