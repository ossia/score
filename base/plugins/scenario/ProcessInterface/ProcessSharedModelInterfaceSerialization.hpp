#pragma once
#include <interface/serialization/DataStreamVisitor.hpp>
class ProcessSharedModelInterface;

ProcessSharedModelInterface* createProcess(Deserializer<DataStream>& deserializer,
										   QObject* parent);

template<>
void Visitor<Reader<DataStream>>::visit(const ProcessSharedModelInterface&);