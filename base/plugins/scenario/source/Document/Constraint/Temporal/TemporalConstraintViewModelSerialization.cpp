#include "TemporalConstraintViewModelSerialization.hpp"
#include "TemporalConstraintViewModel.hpp"

#include "Document/Constraint/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModelSerialization.hpp"

/*
QDataStream& operator <<(QDataStream& s, const TemporalConstraintViewModel& vm)
{
	return s;
}

QDataStream& operator >>(QDataStream& s, TemporalConstraintViewModel& vm)
{
	return s;
}
*/
template<>
void Visitor<Reader<DataStream>>::visit(const TemporalConstraintViewModel& constraint)
{
	visit(static_cast<const AbstractConstraintViewModel&>(constraint));
}
