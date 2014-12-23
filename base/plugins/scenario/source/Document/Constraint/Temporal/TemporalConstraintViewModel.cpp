#include "TemporalConstraintViewModel.hpp"

QDataStream& operator <<(QDataStream& s, const TemporalConstraintViewModel& vm)
{
	return s;
}

QDataStream& operator >>(QDataStream& s, TemporalConstraintViewModel& vm)
{
	return s;
}

void TemporalConstraintViewModel::serialize(QDataStream& s) const
{
	s << *this;
}

TemporalConstraintViewModel::TemporalConstraintViewModel(int id,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{id,
								 "TemporalConstraintViewModel",
								 model,
								 parent}
{

}

TemporalConstraintViewModel::TemporalConstraintViewModel(QDataStream& s,
														 ConstraintModel* model,
														 QObject* parent):
	AbstractConstraintViewModel{s,
								 model,
								 parent}
{
	s >> *this;
}
