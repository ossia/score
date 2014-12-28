#pragma once

#include <interface/serialization/DataStreamVisitor.hpp>

class TemporalScenarioProcessViewModel;

template<>
void Visitor<Reader<DataStream>>::readFrom(const TemporalScenarioProcessViewModel&);
template<>
void Visitor<Writer<DataStream>>::writeTo(TemporalScenarioProcessViewModel&);
