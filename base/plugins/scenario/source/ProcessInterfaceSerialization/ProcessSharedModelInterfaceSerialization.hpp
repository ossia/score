#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
class ProcessSharedModelInterface;

template<typename T>
ProcessSharedModelInterface* createProcess(Deserializer<T>& deserializer,
        QObject* parent);
