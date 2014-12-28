#include "TemporalConstraintViewModelSerialization.hpp"
#include "TemporalConstraintViewModel.hpp"

#include "Document/Constraint/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModelSerialization.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const TemporalConstraintViewModel& constraint)
{
	readFrom(static_cast<const AbstractConstraintViewModel&>(constraint));
}
