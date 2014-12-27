#pragma once
class State;

#include <interface/serialization/DataStreamVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::visit(const State&);
template<>
void Visitor<Writer<DataStream>>::visit(State&);
