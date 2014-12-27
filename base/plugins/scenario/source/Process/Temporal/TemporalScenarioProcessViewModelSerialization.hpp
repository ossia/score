#pragma once

#include <interface/serialization/DataStreamVisitor.hpp>

class TemporalScenarioProcessViewModel;

template<>
void Visitor<Reader<DataStream>>::visit(const TemporalScenarioProcessViewModel&);
template<>
void Visitor<Writer<DataStream>>::visit(TemporalScenarioProcessViewModel&);
