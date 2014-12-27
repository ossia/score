#pragma once
#include "Document/Constraint/ConstraintModel.hpp"

#include <interface/serialization/JSONVisitor.hpp>
#include <interface/serialization/DataStreamVisitor.hpp>

template<>
void Visitor<JSONReader>::visit<ConstraintModel>(ConstraintModel&);
template<>
void Visitor<JSONWriter>::visit<ConstraintModel>(ConstraintModel&);

template<>
void Visitor<DataStreamReader>::visit<ConstraintModel>(ConstraintModel&);
template<>
void Visitor<DataStreamWriter>::visit<ConstraintModel>(ConstraintModel&);