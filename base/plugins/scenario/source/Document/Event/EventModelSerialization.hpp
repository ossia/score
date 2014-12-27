#pragma once
class EventModel;

#include <interface/serialization/DataStreamVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::visit(const EventModel&);
template<>
void Visitor<Writer<DataStream>>::visit(EventModel&);
