#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <ProcesS/ProcessList.hpp>
class Process;

template<typename T>
Process* createProcess(
        const DynamicProcessList&,
        Deserializer<T>& deserializer,
        QObject* parent);
