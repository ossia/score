#include "TemporalConstraintViewModel.hpp"



TemporalConstraintViewModel::TemporalConstraintViewModel(int id, ConstraintModel* model, QObject* parent):
	ConstraintViewModelInterface{id, "TemporalConstraintViewModel", model, parent}
{

}

TemporalConstraintViewModel::TemporalConstraintViewModel(QDataStream& s, ConstraintModel* model, QObject* parent):
	ConstraintViewModelInterface{s, "TemporalConstraintViewModel", model, parent}
{

}
