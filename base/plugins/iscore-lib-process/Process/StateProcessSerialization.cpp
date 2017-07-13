// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
