#include "BaseConstraintViewModel.hpp"

BaseConstraintViewModel::BaseConstraintViewModel(id_type<AbstractConstraintViewModel> id,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{id,
								"BaseConstraintViewModel",
								model,
								parent}
{

}
