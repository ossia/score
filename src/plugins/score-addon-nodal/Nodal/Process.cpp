#include "Process.hpp"

#include <wobjectimpl.h>

#include <Process/ProcessList.hpp>
#include <Nodal/Commands.hpp>
#include <score/tools/std/Invoke.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

W_OBJECT_IMPL(Nodal::Node)
W_OBJECT_IMPL(Nodal::Model)
namespace Nodal
{

Node::Node(std::unique_ptr<Process::ProcessModel> proc,
           const Id<Node>& id, QObject* parent)
  : score::Entity<Node>{id, QStringLiteral("Node"), parent}
  , m_impl{std::move(proc)}
{
  SCORE_ASSERT(m_impl);
  m_impl->setParent(this);
}
Node::Node(no_ownership,
           Process::ProcessModel& proc,
           const Id<Node>& id, QObject* parent)
  : score::Entity<Node>{id, QStringLiteral("Node"), parent}
  , m_impl{&proc}
{
}

Node::~Node()
{

}

QPointF Node::position() const noexcept { return m_position; }

QSizeF Node::size() const noexcept { return m_size; }

Process::ProcessModel& Node::process() const noexcept { return *m_impl; }

void Node::setPosition(const QPointF& v)
{
  if(v != m_position)
  {
    m_position = v;
    positionChanged(v);
  }
}

void Node::setSize(const QSizeF& v)
{
  if(v != m_size)
  {
    m_size = v;
    sizeChanged(v);
  }
}

void Node::release()
{
  m_impl.release();
}





Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "NodalProcess", parent}
    , inlet{Process::make_inlet(Id<Process::Port>(0), this)}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
{
  metadata().setInstanceName(*this);
  inlet->type = Process::PortType::Audio;
  outlet->type = Process::PortType::Audio;
  outlet->setPropagate(true);
  init();
}

Model::~Model()
{
}

QString Model::prettyName() const noexcept
{
  return tr("Nodal Process");
}

void Model::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  for(Node& n : this->nodes)
    n.process().setParentDuration(ExpandMode::Scale, newDuration);
}

void Model::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  for(Node& n : this->nodes)
    n.process().setParentDuration(ExpandMode::GrowShrink, newDuration);
}

void Model::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  for(Node& n : this->nodes)
    n.process().setParentDuration(ExpandMode::GrowShrink, newDuration);
}

void Model::startExecution()
{
}

void Model::stopExecution()
{
  resetExecution();
}

void Model::reset()
{
  resetExecution();
}

bool NodeRemover::remove(const Selection& s, const score::DocumentContext& ctx)
{
  if (s.size() == 1)
  {
    auto first = s.begin()->data();
    if (auto model = qobject_cast<const Process::ProcessModel*>(first))
    {
      if (auto parent = qobject_cast<Model*>(model->parent()->parent()))
      {
        auto f = [&ctx, parent, model] {
          CommandDispatcher<>{ctx.commandStack}.submit<RemoveNode>(
                *parent, *static_cast<Node*>(model->parent()));
        };
        score::invoke(f);
        return true;
      }
    }
  }
  return false;
}

}

template <>
void DataStreamReader::read(const Nodal::Node& proc)
{
  m_stream << *proc.m_impl
           << proc.m_position
           << proc.m_size;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Nodal::Node& node)
{
  static auto& pl = components.interfaces<Process::ProcessFactoryList>();
  auto proc = deserialize_interface(pl, *this, &node);
  if (proc)
  {
    // TODO why isn't AddProcess used here ?!
    node.m_impl.reset(proc);
  }
  else
  {
    SCORE_TODO;
  }
  m_stream >> node.m_position >> node.m_size;

  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Nodal::Node& proc)
{
  obj["Process"] = toJsonObject(*proc.m_impl);
  obj["Pos"] = toJsonValue(proc.m_position);
  obj["Size"] = toJsonValue(proc.m_size);
}

template <>
void JSONObjectWriter::write(Nodal::Node& node)
{
  static auto& pl = components.interfaces<Process::ProcessFactoryList>();

  {
    const auto& json_vref = obj["Process"];
    JSONObject::Deserializer deserializer{json_vref.toObject()};
    auto proc = deserialize_interface(pl, deserializer, &node);
    if (proc)
      node.m_impl.reset(proc);
    else
      SCORE_TODO;
  }
   node.m_position = fromJsonValue<QPointF>(obj["Pos"]);
   node.m_size = fromJsonValue<QSizeF>(obj["Size"]);
}

template <>
void DataStreamReader::read(const Nodal::Model& proc)
{
  // Ports
  m_stream << *proc.inlet << *proc.outlet;

  // Nodes
  m_stream << (int32_t) proc.nodes.size();
  for(const auto& node : proc.nodes)
    readFrom(node);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Nodal::Model& process)
{
  // Ports
  process.inlet = Process::make_inlet(*this, &process);
  process.outlet = Process::make_outlet(*this, &process);

  // Nodes
  int32_t process_count = 0;
  m_stream >> process_count;
  for (; process_count-- > 0;)
  {
    auto node = new Nodal::Node{*this, &process};
    process.nodes.add(node);
  }
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Nodal::Model& proc)
{
  obj["Nodes"] = toJsonArray(proc.nodes);
  obj["Inlet"] = toJsonObject(*proc.inlet);
  obj["Outlet"] = toJsonObject(*proc.outlet);
}

template <>
void JSONObjectWriter::write(Nodal::Model& proc)
{
  {
    JSONObjectWriter writer{obj["Inlet"].toObject()};
    proc.inlet = Process::make_inlet(writer, &proc);
    if (!proc.inlet)
    {
      proc.inlet = Process::make_inlet(Id<Process::Port>(0), &proc);
      proc.inlet->type = Process::PortType::Audio;
    }
  }
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    proc.outlet = Process::make_outlet(writer, &proc);

    if (!proc.outlet)
    {
      proc.outlet = Process::make_outlet(Id<Process::Port>(0), &proc);
      proc.outlet->type = Process::PortType::Audio;
    }
  }
  const auto& nodes = obj["Nodes"].toArray();
  for (const auto& json_vref : nodes)
  {
    auto node = new Nodal::Node{
        JSONObject::Deserializer{json_vref.toObject()}, &proc};

    proc.nodes.add(node);
  }

}
