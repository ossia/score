#include "TemporalConstraintViewModel.hpp"

void TemporalConstraintViewModel::on_boxRemoved(id_type<BoxModel> boxId)
{
	if(shownBox() == boxId)
	{
		hideBox();
		emit boxRemoved();
	}
}

TemporalConstraintViewModel::TemporalConstraintViewModel(id_type<AbstractConstraintViewModel> id,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{id,
								 "TemporalConstraintViewModel",
								 model,
								 parent}
{

}
