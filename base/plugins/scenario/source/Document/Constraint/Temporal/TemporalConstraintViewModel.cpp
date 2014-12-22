#include "TemporalConstraintViewModel.hpp"



TemporalConstraintViewModel::TemporalConstraintViewModel(int id, ConstraintModel* model, QObject* parent):
	IdentifiedObject{id, "TemporalConstraintViewModel", parent},
	m_model{model}
{

}
