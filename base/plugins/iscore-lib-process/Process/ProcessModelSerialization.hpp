#pragma once
#include <iscore/serialization/VisitorInterface.hpp>

class DynamicProcessList;
class Process;
class QObject;

template<typename T>
Process* createProcess(
        const DynamicProcessList&,
        Deserializer<T>& deserializer,
        QObject* parent);
