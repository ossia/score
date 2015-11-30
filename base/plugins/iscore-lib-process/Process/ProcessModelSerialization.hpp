#pragma once
#include <iscore/serialization/VisitorInterface.hpp>

class ProcessList;
class Process;
class QObject;

template<typename T>
Process* createProcess(
        const ProcessList&,
        Deserializer<T>& deserializer,
        QObject* parent);


class StateProcessList;
class StateProcess;
template<typename T>
StateProcess* createStateProcess(
        const StateProcessList&,
        Deserializer<T>& deserializer,
        QObject* parent);

