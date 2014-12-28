#pragma once
class TemporalConstraintViewModel;
#include <interface/serialization/DataStreamVisitor.hpp>


template<>
void Visitor<Reader<DataStream>>::visit(const TemporalConstraintViewModel&);

//template<>
//void Visitor<Writer<DataStream>>::visit(TemporalConstraintViewModel&);