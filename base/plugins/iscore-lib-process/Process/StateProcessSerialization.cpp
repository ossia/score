#include <Process/ProcessList.hpp>
#include <Process/StateProcess.hpp>

template <>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<DataStream>>::read(
    const Process::StateProcess& process)
{
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Process::StateProcess&)
{
  // Delimiter checked on createProcess
}

template <>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<JSONObject>>::readFromConcrete(
    const Process::StateProcess& process)
{
  readFrom(
      static_cast<const IdentifiedObject<Process::StateProcess>&>(process));
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Process::StateProcess& process)
{
}
