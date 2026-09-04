#include <Process/Dataflow/PortFactory.hpp>

#include <Process/OpaqueProcess.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/plugins/UuidKeySerialization.hpp>

#include <QDebug>

namespace Process
{
namespace
{
// deserialize_interface cannot build a stand-in for a missing port, because it
// has no way to tell an inlet from an outlet -- only the caller knows which of
// the two arrays it is filling. So ports are read here instead, with the
// direction supplied as a template argument.
template <typename Opaque_T>
Port* loadOnePort(
    DataStream::Deserializer& des, const PortFactoryList& pl, QObject* parent)
{
  QByteArray b;
  des.stream() >> b;
  DataStream::Deserializer sub{b};

  UuidKey<Process::Port> k;
  TSerializer<DataStream, UuidKey<Process::Port>>::writeTo(sub, k);

  if(auto* fac = pl.get(k))
    return fac->load(sub.toVariant(), parent);

  return new Opaque_T{k, sub, parent};
}

template <typename Opaque_T>
Port* loadOnePort(
    const rapidjson::Value& value, const PortFactoryList& pl, QObject* parent)
{
  JSONObject::Deserializer des{value};

  UuidKey<Process::Port> k;
  {
    JSONWriter wr{des.obj[des.strings.uuid]};
    TSerializer<JSONObject, UuidKey<Process::Port>>::writeTo(wr, k);
  }

  if(auto* fac = pl.get(k))
    return fac->load(des.toVariant(), parent);

  return new Opaque_T{k, des, parent};
}

template <typename Port_T>
void append(ossia::small_vector<Port_T*, 4>& vec, Port* p)
{
  if(p)
    vec.push_back(safe_cast<Port_T*>(p));
  else
    qWarning() << "A port could not be read and was dropped";
}
}

void readPorts(
    DataStreamReader& wr, const Process::Inlets& ins, const Process::Outlets& outs)
{
  wr.m_stream << static_cast<const ossia::small_vector<Process::Inlet*, 4>&>(ins);
  wr.m_stream << static_cast<const ossia::small_vector<Process::Outlet*, 4>&>(outs);
}

void writePorts(
    DataStreamWriter& wr, const Process::PortFactoryList& pl, Process::Inlets& ins,
    Process::Outlets& outs, QObject* parent)
{
  qDeleteAll(ins);
  qDeleteAll(outs);
  ins.clear();
  outs.clear();

  int32_t count{};
  wr.m_stream >> count;
  for(; count-- > 0;)
    append(ins, loadOnePort<OpaqueInlet>(wr, pl, parent));

  wr.m_stream >> count;
  for(; count-- > 0;)
    append(outs, loadOnePort<OpaqueOutlet>(wr, pl, parent));
}
void readPorts(JSONReader& obj, const Process::Inlets& ins, const Process::Outlets& outs)
{
  obj.obj["Inlets"] = static_cast<const ossia::small_vector<Process::Inlet*, 4>&>(ins);
  obj.obj["Outlets"]
      = static_cast<const ossia::small_vector<Process::Outlet*, 4>&>(outs);
}
void writePorts(
    const JSONWriter& obj, const Process::PortFactoryList& pl, Process::Inlets& ins,
    Process::Outlets& outs, QObject* parent)
{
  qDeleteAll(ins);
  qDeleteAll(outs);
  ins.clear();
  outs.clear();

  for(const auto& v : obj.base["Inlets"].GetArray())
    append(ins, loadOnePort<OpaqueInlet>(v, pl, parent));

  for(const auto& v : obj.base["Outlets"].GetArray())
    append(outs, loadOnePort<OpaqueOutlet>(v, pl, parent));
}
}
