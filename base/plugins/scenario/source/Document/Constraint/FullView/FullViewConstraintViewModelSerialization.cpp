#include "FullViewConstraintViewModel.hpp"

#include "Document/Constraint/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/AbstractConstraintViewModelSerialization.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const FullViewConstraintViewModel& constraint)
{
	readFrom(static_cast<const AbstractConstraintViewModel&>(constraint));
}

template<>
void Visitor<Reader<JSON>>::readFrom(const FullViewConstraintViewModel& constraint)
{
	readFrom(static_cast<const AbstractConstraintViewModel&>(constraint));
}
