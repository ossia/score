#include "Port.hpp"
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Process
{

Port::~Port()
{

}

Port::Port(Id<Port> c, const QString& name, QObject* parent)
  : IdentifiedObject<Port>{c, name, parent}
{

}

Port::Port(DataStream::Deserializer& vis, QObject* parent): IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}
Port::Port(JSONObject::Deserializer& vis, QObject* parent): IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}
Port::Port(DataStream::Deserializer&& vis, QObject* parent): IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}
Port::Port(JSONObject::Deserializer&& vis, QObject* parent): IdentifiedObject{vis, parent}
{
  vis.writeTo(*this);
}

void Port::addCable(const Path<Cable> &c)
{
  m_cables.push_back(c);
  emit cablesChanged();
}

void Port::removeCable(const Path<Cable> &c)
{
  auto it = ossia::find(m_cables, c);
  if(it != m_cables.end())
  {
    m_cables.erase(it);
    emit cablesChanged();
  }
}

const QString& Port::customData() const
{
  return m_customData;
}

const State::AddressAccessor& Port::address() const
{
  return m_address;
}

const std::vector<Path<Cable>>& Port::cables() const { return m_cables; }

void Port::setCustomData(const QString &customData)
{
  if (m_customData == customData)
    return;

  m_customData = customData;
  emit customDataChanged(m_customData);
}

void Port::setAddress(const State::AddressAccessor &address)
{
  if (m_address == address)
    return;

  m_address = address;
  emit addressChanged(m_address);
}


///////////////////////////////
/// Inlet
///////////////////////////////

Inlet::~Inlet()
{

}

Inlet::Inlet(Id<Process::Port> c, QObject* parent)
  : Port{std::move(c), QStringLiteral("Inlet"), parent}
{

}

Inlet::Inlet(DataStream::Deserializer& vis, QObject* parent): Port{vis, parent}
{
  vis.writeTo(*this);
}
Inlet::Inlet(JSONObject::Deserializer& vis, QObject* parent): Port{vis, parent}
{
  vis.writeTo(*this);
}
Inlet::Inlet(DataStream::Deserializer&& vis, QObject* parent): Port{vis, parent}
{
  vis.writeTo(*this);
}
Inlet::Inlet(JSONObject::Deserializer&& vis, QObject* parent): Port{vis, parent}
{
  vis.writeTo(*this);
}




ControlInlet::~ControlInlet()
{

}

ControlInlet::ControlInlet(DataStream::Deserializer& vis, QObject* parent): Inlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlInlet::ControlInlet(JSONObject::Deserializer& vis, QObject* parent): Inlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlInlet::ControlInlet(DataStream::Deserializer&& vis, QObject* parent): Inlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlInlet::ControlInlet(JSONObject::Deserializer&& vis, QObject* parent): Inlet{vis, parent}
{
  vis.writeTo(*this);
}






Outlet::~Outlet()
{

}

Outlet::Outlet(Id<Process::Port> c, QObject* parent)
  : Port{std::move(c), QStringLiteral("Outlet"), parent}
{

}

Outlet::Outlet(DataStream::Deserializer& vis, QObject* parent): Port{vis, parent}
{
  vis.writeTo(*this);
}
Outlet::Outlet(JSONObject::Deserializer& vis, QObject* parent): Port{vis, parent}
{
  vis.writeTo(*this);
}
Outlet::Outlet(DataStream::Deserializer&& vis, QObject* parent): Port{vis, parent}
{
  vis.writeTo(*this);
}
Outlet::Outlet(JSONObject::Deserializer&& vis, QObject* parent): Port{vis, parent}
{
  vis.writeTo(*this);
}

bool Outlet::propagate() const
{
  return m_propagate;
}

void Outlet::setPropagate(bool propagate)
{
  if (m_propagate == propagate)
    return;

  m_propagate = propagate;
  emit propagateChanged(m_propagate);
}



ControlOutlet::~ControlOutlet()
{

}

ControlOutlet::ControlOutlet(DataStream::Deserializer& vis, QObject* parent): Outlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlOutlet::ControlOutlet(JSONObject::Deserializer& vis, QObject* parent): Outlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlOutlet::ControlOutlet(DataStream::Deserializer&& vis, QObject* parent): Outlet{vis, parent}
{
  vis.writeTo(*this);
}
ControlOutlet::ControlOutlet(JSONObject::Deserializer&& vis, QObject* parent): Outlet{vis, parent}
{
  vis.writeTo(*this);
}

PortFactory::~PortFactory()
{

}

Port*PortFactoryList::loadMissing(const VisitorVariant& vis, QObject* parent) const
{
  return nullptr;
}
PortFactoryList::~PortFactoryList()
{

}

std::unique_ptr<Inlet> make_inlet(DataStreamWriter& wr, QObject* parent)
{
  static auto& il = score::AppComponents().interfaces<Process::PortFactoryList>();
  auto ptr = deserialize_interface(il, wr, parent);
  return std::unique_ptr<Process::Inlet>((Process::Inlet*) ptr);
}

std::unique_ptr<Inlet> make_inlet(JSONObjectWriter& wr, QObject* parent)
{
  static auto& il = score::AppComponents().interfaces<Process::PortFactoryList>();
  auto ptr = deserialize_interface(il, wr, parent);
  return std::unique_ptr<Process::Inlet>((Process::Inlet*) ptr);
}

std::unique_ptr<Outlet> make_outlet(DataStreamWriter& wr, QObject* parent)
{
  static auto& il = score::AppComponents().interfaces<Process::PortFactoryList>();
  auto ptr = deserialize_interface(il, wr, parent);
  return std::unique_ptr<Process::Outlet>((Process::Outlet*) ptr);
}

std::unique_ptr<Outlet> make_outlet(JSONObjectWriter& wr, QObject* parent)
{
  static auto& il = score::AppComponents().interfaces<Process::PortFactoryList>();
  auto ptr = deserialize_interface(il, wr, parent);
  return std::unique_ptr<Process::Outlet>((Process::Outlet*) ptr);
}

}


template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::Port>(const Process::Port& p)
{
  insertDelimiter();
  m_stream << p.type << p.hidden << p.m_customData << p.m_address;
  insertDelimiter();
}
template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::Port>(Process::Port& p)
{
  checkDelimiter();
  m_stream >> p.type >> p.hidden >> p.m_customData >> p.m_address;
  checkDelimiter();
}

template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::Port>(const Process::Port& p)
{
  obj["Type"] = (int)p.type;
  obj["Hidden"] = (bool)p.hidden;
  obj["Custom"] = p.m_customData;
  obj["Address"] = toJsonObject(p.m_address);
}
template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::Port>(Process::Port& p)
{
  p.type = (Process::PortType)obj["Type"].toInt();
  p.hidden = obj["Hidden"].toBool();
  p.m_customData = obj["Custom"].toString();
  p.m_address = fromJsonObject<State::AddressAccessor>(obj["Address"]);
}


template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::Inlet>(const Process::Inlet& p)
{
  read((Process::Port&)p);
}
template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::Inlet>(Process::Inlet& p)
{
}

template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::Inlet>(const Process::Inlet& p)
{
  read((Process::Port&)p);
}
template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::Inlet>(Process::Inlet& p)
{
}



template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::ControlInlet>(const Process::ControlInlet& p)
{
  //read((Process::Inlet&)p);
  readFrom(p.m_value);
  readFrom(p.m_domain);
}
template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::ControlInlet>(Process::ControlInlet& p)
{
  writeTo(p.m_value);
  writeTo(p.m_domain);
}

template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::ControlInlet>(const Process::ControlInlet& p)
{
  //read((Process::Inlet&)p);
  obj[strings.Value] = toJsonObject(p.m_value);
  obj[strings.Domain] = toJsonObject(p.m_domain);
}
template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::ControlInlet>(Process::ControlInlet& p)
{
  p.m_value = fromJsonObject<ossia::value>(obj[strings.Value]);
  p.m_domain = fromJsonObject<State::Domain>(obj[strings.Domain].toObject());
}



template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::Outlet>(const Process::Outlet& p)
{
  read((Process::Port&)p);
  m_stream << p.m_propagate;
}
template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::Outlet>(Process::Outlet& p)
{
  m_stream >> p.m_propagate;
}

template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::Outlet>(const Process::Outlet& p)
{
  read((Process::Port&)p);
  obj["Propagate"] = p.m_propagate;
}
template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::Outlet>(Process::Outlet& p)
{
  p.m_propagate = obj["Propagate"].toBool();
}



template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::ControlOutlet>(const Process::ControlOutlet& p)
{
  //read((Process::Outlet&)p);
  readFrom(p.m_value);
  readFrom(p.m_domain);
}
template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::ControlOutlet>(Process::ControlOutlet& p)
{
  writeTo(p.m_value);
  writeTo(p.m_domain);
}

template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::ControlOutlet>(const Process::ControlOutlet& p)
{
  //read((Process::Outlet&)p);
  obj[strings.Value] = toJsonValue(p.m_value);
  obj[strings.Domain] = toJsonObject(p.m_domain);
}
template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::ControlOutlet>(Process::ControlOutlet& p)
{
  p.m_value = fromJsonValue<ossia::value>(obj[strings.Value]);
  p.m_domain = fromJsonObject<State::Domain>(obj[strings.Domain].toObject());
}


