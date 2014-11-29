#include "BaseElementModel.hpp"
#include "Interval/IntervalModel.hpp"

BaseElementModel::BaseElementModel(QObject* parent):
	iscore::DocumentDelegateModelInterface{parent, "BaseElementModel"},
	m_baseInterval{new IntervalModel{0, this}}
{
	m_baseInterval->m_width = 1000;
	m_baseInterval->m_height = 1000;
}

