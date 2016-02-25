#include "StateProcess.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const JS::StateProcess& proc)
{
    m_stream << proc.m_script;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(JS::StateProcess& proc)
{
    m_stream >> proc.m_script;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const JS::StateProcess& proc)
{
    m_obj["Script"] = proc.m_script;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(JS::StateProcess& proc)
{
    proc.m_script = m_obj["Script"].toString();
}


namespace JS
{
StateProcess::StateProcess(const Id<Process::StateProcess> &id, QObject *parent):
    Process::StateProcess{id, Metadata<ObjectKey_k, StateProcess>::get(), parent}
{
    m_script = "(function() { \n"
               "     var obj = new Object; \n"
               "     obj[\"address\"] = 'OSCdevice:/millumin/layer/x/instance'; \n"
               "     obj[\"value\"] = Math.sin(iscore.value('OSCdevice:/millumin/layer/y/instance')); \n"
               "     return [ obj ]; \n"
               "});";

}

void StateProcess::setScript(const QString& script)
{
    m_script = script;
    emit scriptChanged(script);
}

void StateProcess::serialize_impl(const VisitorVariant &vis) const
{
    serialize_dyn(vis, *this);
}
}
