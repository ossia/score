#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore_lib_process_export.h>
class ProcessList;
class Process;
class QObject;

template<typename T>
ISCORE_LIB_PROCESS_EXPORT Process* createProcess(
        const ProcessList&,
        Deserializer<T>& deserializer,
        QObject* parent);


class StateProcessList;
class StateProcess;
template<typename T>
ISCORE_LIB_PROCESS_EXPORT StateProcess* createStateProcess(
        const StateProcessList&,
        Deserializer<T>& deserializer,
        QObject* parent);

