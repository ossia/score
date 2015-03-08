#pragma once
#include <public_interface/serialization/DataStreamVisitor.hpp>
#include <public_interface/serialization/JSONVisitor.hpp>
class ProcessSharedModelInterface;

template<typename T>
ProcessSharedModelInterface* createProcess(Deserializer<T>& deserializer,
        QObject* parent);
