#include "BaseElementModel.hpp"
#include "Interval/IntervalModel.hpp"

BaseElementModel::BaseElementModel(QObject* parent):
	iscore::DocumentDelegateModelInterface{parent},
	m_baseInterval{new IntervalModel{0, this}}
{
}

