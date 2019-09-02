#include "Process.hpp"

#include <wobjectimpl.h>

#include <Process/ProcessList.hpp>
#include <Nodal/Commands.hpp>
#include <score/tools/std/Invoke.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

W_OBJECT_IMPL(Nodal::Model)
namespace Nodal
{


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
  for(Process::ProcessModel& n : this->nodes)
    n.setParentDuration(ExpandMode::Scale, newDuration);
}

void Model::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  for(Process::ProcessModel& n : this->nodes)
    n.setParentDuration(ExpandMode::GrowShrink, newDuration);
}

void Model::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  for(Process::ProcessModel& n : this->nodes)
    n.setParentDuration(ExpandMode::GrowShrink, newDuration);
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
                *parent, *static_cast<Process::ProcessModel*>(model->parent()));
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
  static auto& pl = components.interfaces<Process::ProcessFactoryList>();
  int32_t process_count = 0;
  m_stream >> process_count;
  for (; process_count-- > 0;)
  {
    auto proc = deserialize_interface(pl, *this, &process);
    if (proc)
    {
      // TODO why isn't AddProcess used here ?!
      process.nodes.add(proc);
    }
    else
    {
      SCORE_TODO;
    }
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

  static auto& pl = components.interfaces<Process::ProcessFactoryList>();
  const auto& nodes = obj["Nodes"].toArray();
  for (const auto& json_vref : nodes)
  {
    JSONObject::Deserializer deserializer{json_vref.toObject()};
    auto p = deserialize_interface(pl, deserializer, &proc);
    if (p)
      proc.nodes.add(p);
    else
      SCORE_TODO;
  }
}
