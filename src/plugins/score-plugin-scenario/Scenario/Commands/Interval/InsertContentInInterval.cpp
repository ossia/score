// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "InsertContentInInterval.hpp"

#include <Process/ExpandMode.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Scenario/ScenarioPaste.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace Scenario
{
namespace Command
{

QRectF copiedProcessesRect(const rapidjson::Value::Array& sourceProcesses)
{
  QPointF top_left{1e8, 1e8}, bottom_right{-1e8, -1e8};
  for(std::size_t i = 0; i < sourceProcesses.Size(); i++)
  {
    const auto& obj = sourceProcesses[i].GetObject();
    auto pos = obj["Pos"].GetArray();
    auto sz = obj["Size"].GetArray();
    double x = pos[0].GetDouble();
    double y = pos[1].GetDouble();
    double w = sz[0].GetDouble();
    double h = sz[1].GetDouble();
    if(x < top_left.x())
      top_left.rx() = x;
    if(y < top_left.y())
      top_left.ry() = y;
    if(x + w > bottom_right.x())
      bottom_right.rx() = x + w;
    if(y + h > bottom_right.y())
      bottom_right.ry() = y + h;
  }
  return QRectF{top_left, bottom_right};
}

PasteProcessesInInterval::PasteProcessesInInterval(
    rapidjson::Value::Array sourceProcesses, rapidjson::Value::Array sourceCables,
    const IntervalModel& targetInterval, ExpandMode mode,
    QPointF p)
    : // m_cables{clone(sourceCables)}
    m_target{std::move(targetInterval)}
    , m_mode{mode}
    , m_origin{p}
{
  auto& ctx = targetInterval.context();

  // Generate new ids for each cloned process.
  auto [processes, processes_ids]
      = ProcessesBeingCopied{sourceProcesses, targetInterval, ctx};

  // Generate new ids for cables to create
  auto cables = Scenario::cableDataFromCablesJson(sourceCables);
  m_cables.cables
      = mapCopiedCables(ctx, cables, processes, processes_ids, targetInterval);

  // Compute the rect of the objects
  QRectF copyRect = copiedProcessesRect(sourceProcesses);
  QPointF center = copyRect.center();

  // Adjust the deserialized processes
  int i = 0;
  for(Process::ProcessModel* obj : processes)
  {
    // Adapt the id
    obj->setId(processes_ids[i]);

    // Put in the updated rect
    double x = obj->position().x();
    double y = obj->position().y();
    double w = obj->size().width();
    double h = obj->size().height();
    QRectF itemRect{x, y, w, h};

    double nx = p.x() + center.x() - x - itemRect.width();
    double ny = p.y() + center.y() - y - itemRect.height();

    obj->setPosition(QPointF{nx, ny});

    i++;
  }

  m_ids_processes.reserve(processes.size());
  m_json_processes.reserve(processes.size());
  for(auto elt : processes)
  {
    m_ids_processes.push_back(elt->id());
    m_json_processes.push_back(score::marshall<DataStream>(*elt));

    delete elt;
  }
}

void PasteProcessesInInterval::undo(const score::DocumentContext& ctx) const
{
  auto& trg_interval = m_target.find(ctx);
  // We just have to remove what we added
  // TODO Remove the added slots, etc.

  // Remove the cables
  m_cables.undo(ctx);

  // Remove the processes
  for(const auto& proc_id : m_ids_processes)
  {
    RemoveProcess(trg_interval, proc_id);
  }

  if(trg_interval.processes.empty())
    trg_interval.setSmallViewVisible(false);
}

void PasteProcessesInInterval::redo(const score::DocumentContext& ctx) const
{
  auto& pl = ctx.app.components.interfaces<Process::ProcessFactoryList>();
  auto& trg_interval = m_target.find(ctx);
  std::vector<Id<Process::ProcessModel>> processesToPutInSlot;

  for(const auto& proc : m_json_processes)
  {
    DataStream::Deserializer deserializer{proc};
    auto newproc = deserialize_interface(pl, deserializer, ctx, &trg_interval);
    if(newproc)
    {
      AddProcess(trg_interval, newproc);

      if(!(newproc->flags() & Process::ProcessFlags::TimeIndependent))
      {
        processesToPutInSlot.push_back(newproc->id());
      }

      // Resize the processes according to the new interval.
      if(m_mode == ExpandMode::Scale)
      {
        newproc->setParentDuration(
            ExpandMode::Scale, trg_interval.duration.defaultDuration());
      }
      else if(m_mode == ExpandMode::GrowShrink)
      {
        newproc->setParentDuration(
            ExpandMode::ForceGrow, trg_interval.duration.defaultDuration());
      }
    }
    else
      SCORE_TODO;
  }

  for(auto& p : processesToPutInSlot)
  {
    Slot s{{p}, p, false};
    trg_interval.addSlot(s);
  }
  if(m_json_processes.size() > 0 && !trg_interval.smallViewVisible())
    trg_interval.setSmallViewVisible(true);

  // Add cables
  m_cables.redo(ctx);
}

void PasteProcessesInInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_target << (int)m_mode << m_origin << m_ids_processes << m_json_processes
    << m_cables.cables;
}

void PasteProcessesInInterval::deserializeImpl(DataStreamOutput& s)
{
  int mode{};
  s >> m_target >> mode >> m_origin >> m_ids_processes >> m_json_processes
      >> m_cables.cables;
  m_mode = static_cast<ExpandMode>(mode);
}
}
}
