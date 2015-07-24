#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
class Process;

template<typename T>
Process* createProcess(Deserializer<T>& deserializer,
        QObject* parent);
