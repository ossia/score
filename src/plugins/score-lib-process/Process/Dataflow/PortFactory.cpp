#include <Process/Dataflow/PortFactory.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>

#include <QDebug>

namespace Process
{
namespace
{
// A saved port whose type has no factory any more (the plug-in that provided
// it is missing, or the process now declares another kind of port at that
// place, e.g. an avnd control that became a time chooser) is loaded as the
// generic port of its direction. It keeps its id, which the document's cables
// and the process's port mapping refer to, its name, address and exposed
// name; from a JSON save, a control also keeps its value and domain. The
// process can then rebuild it as the port it declares now.
void warnUnknownPort(const QByteArray& key, const QString& name, QObject* parent)
{
  qWarning() << "Unknown port type" << key << "for port" << name << "of"
             << (parent ? parent->metaObject()->className() : "(no parent)")
             << ": loaded as a generic port";
}

template <typename Value_T>
Port* loadPort(DataStreamWriter& wr, const PortFactoryList& pl, QObject* parent)
{
  QByteArray bytes;
  wr.m_stream >> bytes;
  DataStream::Deserializer sub{bytes};
  try
  {
    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    PortFactory::ConcreteKey k;
    TSerializer<DataStream, PortFactory::ConcreteKey>::writeTo(sub, k);
    SCORE_DEBUG_CHECK_DELIMITER2(sub);

    if(auto factory = pl.get(k))
    {
      auto port = factory->load(sub.toVariant(), parent);
      SCORE_DEBUG_CHECK_DELIMITER2(sub);
      return port;
    }

    // Every port's serialization starts with the Port base; what follows
    // depends on the unknown type and is skipped.
    auto port = new Value_T{sub, parent};
    warnUnknownPort(score::uuids::toByteArray(k.impl()), port->name(), parent);
    return port;
  }
  catch(...)
  {
    return nullptr;
  }
}

template <typename Control_T, typename Value_T>
Port* loadPort(const rapidjson::Value& json, const PortFactoryList& pl, QObject* parent)
{
  if(!json.IsObject())
    return nullptr;

  JSONWriter wr{json};
  try
  {
    if(auto port = deserialize_interface(pl, wr, parent))
      return port;

    if(!wr.obj.tryGet(wr.strings.id))
      return nullptr;

    Port* port{};
    if(wr.obj.tryGet(wr.strings.Value) && wr.obj.tryGet(wr.strings.Domain))
      port = new Control_T{wr, parent};
    else
      port = new Value_T{wr, parent};

    QByteArray key;
    if(auto uuid = wr.obj.tryGet(wr.strings.uuid))
      key = uuid->toByteArray();
    warnUnknownPort(key, port->name(), parent);
    return port;
  }
  catch(...)
  {
    return nullptr;
  }
}

void warnUnreadablePort(QObject* parent)
{
  qWarning() << "Unreadable port of"
             << (parent ? parent->metaObject()->className() : "(no parent)")
             << ": skipped";
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
  for(; count > 0; --count)
  {
    if(auto port = loadPort<Process::ValueInlet>(wr, pl, parent))
      ins.push_back(safe_cast<Process::Inlet*>(port));
    else
      warnUnreadablePort(parent);
  }

  wr.m_stream >> count;
  for(; count > 0; --count)
  {
    if(auto port = loadPort<Process::ValueOutlet>(wr, pl, parent))
      outs.push_back(safe_cast<Process::Outlet*>(port));
    else
      warnUnreadablePort(parent);
  }
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

  if(auto inlets = obj.obj.tryGet("Inlets"); inlets && inlets->obj.IsArray())
  {
    for(const auto& json : inlets->obj.GetArray())
    {
      if(auto port
         = loadPort<Process::ControlInlet, Process::ValueInlet>(json, pl, parent))
        ins.push_back(safe_cast<Process::Inlet*>(port));
      else
        warnUnreadablePort(parent);
    }
  }

  if(auto outlets = obj.obj.tryGet("Outlets"); outlets && outlets->obj.IsArray())
  {
    for(const auto& json : outlets->obj.GetArray())
    {
      if(auto port
         = loadPort<Process::ControlOutlet, Process::ValueOutlet>(json, pl, parent))
        outs.push_back(safe_cast<Process::Outlet*>(port));
      else
        warnUnreadablePort(parent);
    }
  }
}
}
