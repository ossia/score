#pragma once
#include <interface/serialization/DataStreamVisitor.hpp>
#include <interface/serialization/JSONVisitor.hpp>
class ProcessSharedModelInterface;

template<typename T>
ProcessSharedModelInterface* createProcess (Deserializer<T>& deserializer,
        QObject* parent);
