#include "TemporalConstraintViewModel.hpp"

#include "Document/Constraint/ViewModels/AbstractConstraintViewModelSerialization.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const TemporalConstraintViewModel& constraint)
{
    readFrom(static_cast<const AbstractConstraintViewModel&>(constraint));
}

template<>
void Visitor<Reader<JSON>>::readFrom(const TemporalConstraintViewModel& constraint)
{
    readFrom(static_cast<const AbstractConstraintViewModel&>(constraint));
}
