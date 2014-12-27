#pragma once

#include "interface/serialization/DataStreamVisitor.hpp"
class IdentifiedObject;


template<>
void Visitor<Reader<DataStream>>::visit<IdentifiedObject>(IdentifiedObject&);
template<>
void Visitor<Writer<DataStream>>::visit<IdentifiedObject>(IdentifiedObject&);
