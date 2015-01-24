#include "TemporalConstraintViewModel.hpp"

TemporalConstraintViewModel::TemporalConstraintViewModel(id_type<AbstractConstraintViewModel> id,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{id,
								"TemporalConstraintViewModel",
								model,
								parent}
{

}
