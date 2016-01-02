#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore_lib_process_export.h>
class QObject;

namespace Process {

class ProcessList;
template<typename T>
ISCORE_LIB_PROCESS_EXPORT Process::ProcessModel* createProcess(
        const ProcessList&,
        Deserializer<T>& deserializer,
        QObject* parent);


class StateProcessList;
class StateProcess;
template<typename T>
ISCORE_LIB_PROCESS_EXPORT Process::StateProcess* createStateProcess(
        const StateProcessList&,
        Deserializer<T>& deserializer,
        QObject* parent);

}
