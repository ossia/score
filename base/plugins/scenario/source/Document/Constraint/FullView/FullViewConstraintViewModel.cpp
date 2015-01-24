#include "FullViewConstraintViewModel.hpp"

void FullViewConstraintViewModel::on_boxRemoved(id_type<BoxModel> boxId)
{
	if(shownBox() == boxId)
	{
		hideBox();
		emit boxRemoved();
	}
}

FullViewConstraintViewModel::FullViewConstraintViewModel(id_type<AbstractConstraintViewModel> id,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{id,
								 "FullViewConstraintViewModel",
								 model,
								 parent}
{

}
