#include "JSStateProcess.hpp"


template <>
void DataStreamReader::read(const JS::StateProcess& proc)
{
  m_stream << proc.m_script;
  insertDelimiter();
}


template <>
void DataStreamWriter::writeTo(JS::StateProcess& proc)
{
  m_stream >> proc.m_script;
  checkDelimiter();
}


template <>
void JSONObjectReader::readFromConcrete(const JS::StateProcess& proc)
{
  obj["Script"] = proc.m_script;
}


template <>
void JSONObjectWriter::writeTo(JS::StateProcess& proc)
{
  proc.m_script = obj["Script"].toString();
}

namespace JS
{
StateProcess::StateProcess(
    const Id<Process::StateProcess>& id, QObject* parent)
    : Process::StateProcess{id, Metadata<ObjectKey_k, StateProcess>::get(),
                            parent}
{
  m_script
      = "(function() { \n"
        "     var obj = new Object; \n"
        "     obj[\"address\"] = 'OSCdevice:/millumin/layer/x/instance'; \n"
        "     obj[\"value\"] = "
        "Math.sin(iscore.value('OSCdevice:/millumin/layer/y/instance')); \n"
        "     return [ obj ]; \n"
        "});";
}

void StateProcess::setScript(const QString& script)
{
  m_script = script;
  emit scriptChanged(script);
}
}
