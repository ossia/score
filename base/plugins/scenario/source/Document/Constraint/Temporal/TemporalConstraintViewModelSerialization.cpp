#include "TemporalConstraintViewModelSerialization.hpp"
#include "TemporalConstraintViewModel.hpp"

#include "Document/Constraint/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModelSerialization.hpp"

template<>
void Visitor<Reader<DataStream>>::visit(const TemporalConstraintViewModel& constraint)
{
	visit(static_cast<const AbstractConstraintViewModel&>(constraint));
}
