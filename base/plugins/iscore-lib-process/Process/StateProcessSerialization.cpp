#include <Process/StateProcess.hpp>
#include <Process/ProcessList.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Process::StateProcess& process)
{
    readFrom(static_cast<const IdentifiedObject<Process::StateProcess>&>(process));
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Process::StateProcess&)
{
    // Delimiter checked on createProcess
}


template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Process::StateProcess& process)
{
    readFrom(static_cast<const IdentifiedObject<Process::StateProcess>&>(process));
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Process::StateProcess& process)
{
}
