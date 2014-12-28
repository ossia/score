#pragma once
#include <interface/serialization/DataStreamVisitor.hpp>

class ScenarioProcessSharedModel;

template<>
void Visitor<Reader<DataStream>>::readFrom(const ScenarioProcessSharedModel&);
template<>
void Visitor<Writer<DataStream>>::writeTo(ScenarioProcessSharedModel&);
