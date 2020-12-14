#include "Process.hpp"

#include <Process/ProcessList.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMapSerialization.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/tools/std/Invoke.hpp>

#include <Nodal/Commands.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Nodal::Model)
namespace Nodal
{

Model::Model(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    const score::DocumentContext& ctx,
    QObject* parent)
    : Process::ProcessModel{duration, id, "NodalProcess", parent}
    , inlet{Process::make_audio_inlet(Id<Process::Port>(0), this)}
    , outlet{Process::make_audio_outlet(Id<Process::Port>(0), this)}
    , m_context{ctx}
{
  metadata().setInstanceName(*this);
  outlet->setPropagate(true);
  init();
}

Model::~Model() { }

QString Model::prettyName() const noexcept
{
  return tr("Nodal Process");
}

void Model::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  for (Process::ProcessModel& n : this->nodes)
    n.setParentDuration(ExpandMode::Scale, newDuration);
}

void Model::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  for (Process::ProcessModel& n : this->nodes)
    n.setParentDuration(ExpandMode::GrowShrink, newDuration);
}

void Model::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  for (Process::ProcessModel& n : this->nodes)
    n.setParentDuration(ExpandMode::GrowShrink, newDuration);
}

void Model::ancestorStartDateChanged()
{
  for (Process::ProcessModel& n : this->nodes)
    n.ancestorStartDateChanged();
}

void Model::ancestorTempoChanged()
{
  for (Process::ProcessModel& n : this->nodes)
    n.ancestorTempoChanged();
}

bool NodeRemover::remove(const Selection& s, const score::DocumentContext& ctx)
{
  if (s.size() == 1)
  {
    auto first = s.begin()->data();
    if (auto model = qobject_cast<const Process::ProcessModel*>(first))
    {
      if (auto parent = qobject_cast<Model*>(model->parent()))
      {
        auto f = [&ctx, parent, model] {
          CommandDispatcher<>{ctx.commandStack}.submit<RemoveNode>(*parent, *model);
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
  m_stream << (int32_t)proc.nodes.size();
  for (const auto& node : proc.nodes)
    readFrom(node);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Nodal::Model& process)
{
  // Ports
  process.inlet = Process::load_audio_inlet(*this, &process);
  process.outlet = Process::load_audio_outlet(*this, &process);

  // Nodes
  static auto& pl = components.interfaces<Process::ProcessFactoryList>();
  int32_t process_count = 0;
  m_stream >> process_count;
  for (; process_count-- > 0;)
  {
    auto proc = deserialize_interface(pl, *this, process.m_context, &process);
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
void JSONReader::read(const Nodal::Model& proc)
{
  obj["Nodes"] = proc.nodes;
  obj["Inlet"] = *proc.inlet;
  obj["Outlet"] = *proc.outlet;
}

template <>
void JSONWriter::write(Nodal::Model& proc)
{
  {
    JSONWriter writer{obj["Inlet"]};
    proc.inlet = Process::load_audio_inlet(writer, &proc);
  }
  {
    JSONWriter writer{obj["Outlet"]};
    proc.outlet = Process::load_audio_outlet(writer, &proc);
  }

  static auto& pl = components.interfaces<Process::ProcessFactoryList>();
  const auto& nodes = obj["Nodes"].toArray();
  for (const auto& json_vref : nodes)
  {
    JSONObject::Deserializer deserializer{json_vref};
    auto p = deserialize_interface(pl, deserializer, proc.m_context, &proc);
    if (p)
      proc.nodes.add(p);
    else
      SCORE_TODO;
  }
}
