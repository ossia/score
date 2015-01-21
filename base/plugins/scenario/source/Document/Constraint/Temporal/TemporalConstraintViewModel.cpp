#include "TemporalConstraintViewModel.hpp"

void TemporalConstraintViewModel::on_boxRemoved(int boxId)
{
	if(shownBox() == boxId)
	{
		hideBox();
		emit boxRemoved();
	}
}

TemporalConstraintViewModel::TemporalConstraintViewModel(id_type id,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{id,
								 "TemporalConstraintViewModel",
								 model,
								 parent}
{

}
