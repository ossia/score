#pragma once
class ConstraintModel;
#include <interface/serialization/DataStreamVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::visit(const ConstraintModel&);
template<>
void Visitor<Writer<DataStream>>::visit(ConstraintModel&);