#include <Process/ProcessList.hpp>
#include <Process/StateProcess.hpp>

template <>
ISCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(
    const Process::StateProcess& process)
{
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::StateProcess&)
{
  // Delimiter checked on createProcess
}

template <>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read(
    const Process::StateProcess& process)
{
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write(Process::StateProcess& process)
{
}
