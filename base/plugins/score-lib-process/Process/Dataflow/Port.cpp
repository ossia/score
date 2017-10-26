#include "Port.hpp"

namespace Process
{

Port::~Port()
{

}

Port::Port(Id<Port> c, QObject* parent)
  : IdentifiedObject<Port>{c, QStringLiteral("Port"), parent}
{

}

Port::Port(Id<Port> c, const Port& other, QObject* parent)
  : IdentifiedObject<Port>{c, QStringLiteral("Port"), parent}
{
  type = other.type;
  m_propagate = other.m_propagate;
  outlet = other.outlet;
  m_cables = other.m_cables;
  m_customData = other.m_customData;
  m_address = other.m_address;
}

Port* Port::clone(QObject* parent) const
{
  return new Port{id(), *this, parent};
}

void Port::addCable(const Id<Cable> &c)
{
  m_cables.push_back(c);
  emit cablesChanged();
}

void Port::removeCable(const Id<Cable> &c)
{
  auto it = ossia::find(m_cables, c);
  if(it != m_cables.end())
  {
    m_cables.erase(it);
    emit cablesChanged();
  }
}

QString Port::customData() const
{
  return m_customData;
}

State::AddressAccessor Port::address() const
{
  return m_address;
}

const std::vector<Id<Cable> > &Port::cables() const { return m_cables; }

bool Port::propagate() const
{
  return m_propagate;
}

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

void Port::setPropagate(bool propagate)
{
  if (m_propagate == propagate)
    return;

  m_propagate = propagate;
  emit propagateChanged(m_propagate);
}

}


template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::Port>(const Process::Port& p)
{
  insertDelimiter();
  m_stream << p.type << p.m_propagate << p.outlet << p.m_customData << p.m_address << p.m_cables;
  insertDelimiter();
}
template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::Port>(Process::Port& p)
{
  checkDelimiter();
  m_stream >> p.type >> p.m_propagate >> p.outlet >> p.m_customData >> p.m_address >> p.m_cables;
  checkDelimiter();
}

template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::Port>(const Process::Port& p)
{
  obj["Type"] = (int)p.type;
  obj["Propagate"] = p.m_propagate;
  obj["Outlet"] = p.outlet;
  obj["Custom"] = p.m_customData;
  obj["Address"] = toJsonObject(p.m_address);
  obj["Cables"] = toJsonValueArray(p.m_cables);
}
template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::Port>(Process::Port& p)
{
  p.type = (Process::PortType)obj["Type"].toInt();
  p.m_propagate = obj["Propagate"].toBool();
  p.outlet = obj["Outlet"].toBool();
  p.m_customData = obj["Custom"].toString();
  p.m_address = fromJsonObject<State::AddressAccessor>(obj["Address"]);
  fromJsonValueArray(obj["Cables"].toArray(), p.m_cables);
}
