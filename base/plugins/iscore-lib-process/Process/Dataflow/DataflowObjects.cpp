#include "DataflowObjects.hpp"
#include <Process/Dataflow/DataflowProcess.hpp>
namespace Process
{

Cable::~Cable()
{

}

Cable::Cable(Id<Cable> c): IdentifiedObject{c, "Cable", nullptr} { }

Cable::Cable(const iscore::DocumentContext& ctx, Id<Cable> c, const CableData& data):
  IdentifiedObject{c, "Cable", nullptr}
{
  m_type = data.type;
  m_source = &data.source.find(ctx);
  m_sink = &data.sink.find(ctx);
  m_outlet = data.outlet;
  m_inlet = data.inlet;
}

CableType Cable::type() const
{
  return m_type;
}

DataflowProcess*Cable::source() const
{
  return m_source;
}

DataflowProcess*Cable::sink() const
{
  return m_sink;
}

ossia::optional<int> Cable::outlet() const
{
  return m_outlet;
}

ossia::optional<int> Cable::inlet() const
{
  return m_inlet;
}

void Cable::setType(CableType type)
{
  if (m_type == type)
    return;

  m_type = type;
  emit typeChanged(m_type);
}

void Cable::setSource(DataflowProcess* source)
{
  if (m_source == source)
    return;

  m_source = source;
  emit sourceChanged(m_source);
}

void Cable::setSink(DataflowProcess* sink)
{
  if (m_sink == sink)
    return;

  m_sink = sink;
  emit sinkChanged(m_sink);
}

void Cable::setOutlet(ossia::optional<int> outlet)
{
  if (m_outlet == outlet)
    return;

  m_outlet = outlet;
  emit outletChanged(m_outlet);
}

void Cable::setInlet(ossia::optional<int> inlet)
{
  if (m_inlet == inlet)
    return;

  m_inlet = inlet;
  emit inletChanged(m_inlet);
}

}
