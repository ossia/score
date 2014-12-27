#pragma once
#include <interface/serialization/DataStreamVisitor.hpp>
#include "ProcessSharedModelInterface.hpp"


ProcessSharedModelInterface* createProcess(Deserializer<DataStream>& deserializer,
										   QObject* parent);

template<>
void Visitor<Reader<DataStream>>::visit<ProcessSharedModelInterface>(ProcessSharedModelInterface&);