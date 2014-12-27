#pragma once
#include <interface/serialization/DataStreamVisitor.hpp>

class ScenarioProcessSharedModel;

template<>
void Visitor<Reader<DataStream>>::visit(const ScenarioProcessSharedModel&);
template<>
void Visitor<Writer<DataStream>>::visit(ScenarioProcessSharedModel&);
