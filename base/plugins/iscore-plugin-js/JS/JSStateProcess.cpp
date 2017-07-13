// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JSStateProcess.hpp"

#include <QJSEngine>

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

  QJSEngine engine;
  auto f = engine.evaluate(script);
  if(f.isError())
  {
    emit scriptError(f.property("lineNumber").toInt(), f.toString());
  }
  else
  {
    emit scriptOk();
  }

  emit scriptChanged(script);
}
}

template <>
void DataStreamReader::read(const JS::StateProcess& proc)
{
  m_stream << proc.m_script;
  insertDelimiter();
}


template <>
void DataStreamWriter::write(JS::StateProcess& proc)
{
  m_stream >> proc.m_script;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(const JS::StateProcess& proc)
{
  obj["Script"] = proc.m_script;
}


template <>
void JSONObjectWriter::write(JS::StateProcess& proc)
{
  proc.m_script = obj["Script"].toString();
}
