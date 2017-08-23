#include "DataflowObjects.hpp"
#include <Process/Dataflow/DataflowProcess.hpp>
#include <ossia/dataflow/graph_node.hpp>
namespace Dataflow
{
ProcessComponent::ProcessComponent(
    Process::ProcessModel& process,
    DocumentPlugin& doc,
    const Id<iscore::Component>& id,
    const QString& name,
    QObject* parent):
  Process::GenericProcessComponent<DocumentPlugin>{process, doc, id, name, parent}
{

}
ProcessComponent::~ProcessComponent()
{
}

}
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
  m_source = data.source.try_find(ctx);
  m_sink = data.sink.try_find(ctx);
  m_outlet = data.outlet;
  m_inlet = data.inlet;
}

void Cable::update(const iscore::DocumentContext& ctx, const CableData& data)
{
  source_node.reset();
  sink_node.reset();
  exec.reset();

  setType(data.type);
  setSource(data.source.try_find(ctx));
  setSink(data.sink.try_find(ctx));
  setOutlet(data.outlet);
  setInlet(data.inlet);
}

CableData Cable::toCableData() const
{
  CableData c;
  c.type = m_type;
  if(m_source) c.source = *m_source;
  if(m_sink) c.sink = *m_sink;
  c.outlet = m_outlet;
  c.inlet = m_inlet;

  return c;
}

CableType Cable::type() const
{
  return m_type;
}

Process::Node* Cable::source() const
{
  return m_source;
}

Process::Node* Cable::sink() const
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

void Cable::setSource(Process::Node* source)
{
  if (m_source == source)
    return;
  if(m_source)
  {
    QObject::disconnect(m_srcDeath);
    m_source->removeCable(id());
  }

  m_source = source;

  if(m_source)
  {
    m_srcDeath = connect(m_source, &QObject::destroyed, this, [=] {
      setSource(nullptr);
    });
    m_source->addCable(id());
  }

  emit sourceChanged(m_source);
}

void Cable::setSink(Process::Node* sink)
{
  if (m_sink == sink)
    return;
  if(m_sink)
  {
    QObject::disconnect(m_sinkDeath);
    m_sink->removeCable(id());
  }

  m_sink = sink;

  if(m_sink)
  {
    m_sinkDeath = connect(m_sink, &QObject::destroyed, this, [=] {
      setSink(nullptr);
    });

    m_sink->addCable(id());
  }

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

Node::Node(Id<Node> c, QObject* parent)
  : Entity{std::move(c), QStringLiteral("Node"), parent}
{

}

Node::Node(Id<Node> c, QString name, QObject* parent)
  : Entity{std::move(c), std::move(name), parent}
{

}

Node::~Node()
{
  if(exec)
    exec->clear();
  delete ui;
}

}
