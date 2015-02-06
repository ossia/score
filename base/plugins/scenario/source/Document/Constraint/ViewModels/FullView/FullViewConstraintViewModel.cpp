#include "FullViewConstraintViewModel.hpp"

FullViewConstraintViewModel::FullViewConstraintViewModel(id_type<AbstractConstraintViewModel> id,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{id,
								"FullViewConstraintViewModel",
								model,
								parent}
{

}
