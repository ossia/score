#pragma once
class BoxModel;
#include <interface/serialization/DataStreamVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::visit(const BoxModel&);
template<>
void Visitor<Writer<DataStream>>::visit(BoxModel&);