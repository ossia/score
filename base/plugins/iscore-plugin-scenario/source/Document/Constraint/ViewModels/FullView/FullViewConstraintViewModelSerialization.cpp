#include "FullViewConstraintViewModel.hpp"

#include "Document/Constraint/ViewModels/AbstractConstraintViewModelSerialization.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const FullViewConstraintViewModel& constraint)
{
    readFrom(static_cast<const ConstraintViewModel&>(constraint));
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const FullViewConstraintViewModel& constraint)
{
    readFrom(static_cast<const ConstraintViewModel&>(constraint));
}
