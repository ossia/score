#pragma once
class DeckModel;
#include <interface/serialization/DataStreamVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::visit(const DeckModel&);
template<>
void Visitor<Writer<DataStream>>::visit(DeckModel&);