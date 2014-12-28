#include "TemporalConstraintViewModel.hpp"

void TemporalConstraintViewModel::on_boxRemoved(int boxId)
{
	if(shownBox() == boxId)
	{
		hideBox();
		emit boxRemoved();
	}
}

/*
void TemporalConstraintViewModel::serialize(QDataStream& s) const
{
	s << *this;
}*/

TemporalConstraintViewModel::TemporalConstraintViewModel(int id,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{id,
								 "TemporalConstraintViewModel",
								 model,
								 parent}
{

}

/*
TemporalConstraintViewModel::TemporalConstraintViewModel(QDataStream& s,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{s,
								 model,
								 parent}
{
	s >> *this;
}
*/
