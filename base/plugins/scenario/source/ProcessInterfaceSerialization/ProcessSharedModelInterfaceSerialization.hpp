#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
class ProcessModel;

template<typename T>
ProcessModel* createProcess(Deserializer<T>& deserializer,
        QObject* parent);
