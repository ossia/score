#include "ProcessModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortSerialization.hpp>
#include <Process/TimeValue.hpp>

#include <score/model/EntityMapSerialization.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

W_OBJECT_IMPL(ClipLauncher::ProcessModel)

namespace ClipLauncher
{

ProcessModel::ProcessModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id,
    const score::DocumentContext& ctx, QObject* parent)
    : Process::ProcessModel{duration, id, "ClipLauncher", parent}
    , m_context{ctx}
{
  metadata().setInstanceName(*this);
  inlet = std::make_unique<Process::AudioInlet>("Audio In", Id<Process::Port>(0), this);
  outlet = std::make_unique<Process::AudioOutlet>("Audio Out", Id<Process::Port>(0), this);
  outlet->setPropagate(true);

  // Create initial grid: 2 lanes x 4 scenes with cells at every position
  for(int l = 0; l < 2; l++)
  {
    auto lane = new LaneModel{getStrongId(lanes), this};
    lane->setName(QString("Lane %1").arg(l + 1));
    lanes.add(lane);
  }
  for(int s = 0; s < 4; s++)
  {
    auto scene = new SceneModel{getStrongId(scenes), this};
    scene->setName(QString("Scene %1").arg(s + 1));
    scenes.add(scene);
  }
  for(int l = 0; l < 2; l++)
    for(int s = 0; s < 4; s++)
      cells.add(createDefaultCell(getStrongId(cells), l, s, ctx, this));

  init();
}

ProcessModel::~ProcessModel() { }

void ProcessModel::init()
{
  m_inlets.push_back(inlet.get());
  m_outlets.push_back(outlet.get());
}

CellModel* ProcessModel::cellAt(int lane, int scene) const
{
  for(auto& cell : cells)
  {
    if(cell.lane() == lane && cell.scene() == scene)
      return &cell;
  }
  return nullptr;
}

std::vector<CellModel*> ProcessModel::cellsInLane(int lane) const
{
  std::vector<CellModel*> result;
  for(auto& cell : cells)
  {
    if(cell.lane() == lane)
      result.push_back(&cell);
  }
  return result;
}

std::vector<CellModel*> ProcessModel::cellsInScene(int scene) const
{
  std::vector<CellModel*> result;
  for(auto& cell : cells)
  {
    if(cell.scene() == scene)
      result.push_back(&cell);
  }
  return result;
}

void ProcessModel::setGlobalQuantization(double q)
{
  if(m_globalQuantization != q)
  {
    m_globalQuantization = q;
    globalQuantizationChanged(q);
  }
}

CellModel* ProcessModel::createDefaultCell(
    const Id<CellModel>& id, int lane, int scene,
    const score::DocumentContext& ctx, QObject* parent)
{
  auto cell = new CellModel{id, ctx, parent};
  cell->setLane(lane);
  cell->setScene(scene);

  // Clip-launcher defaults: 8s, flexible, end trigger active
  auto& dur = cell->interval().duration;
  const auto defaultDur = TimeVal::fromMsecs(8000);
  dur.setDefaultDuration(defaultDur);
  dur.setRigid(false);
  dur.setMinNull(true);
  dur.setMaxInfinite(true);
  cell->endTimeSync().setDate(defaultDur);
  cell->endTimeSync().setActive(true);

  return cell;
}

void ProcessModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  setDuration(newDuration);
}

Selection ProcessModel::selectableChildren() const noexcept
{
  Selection s;
  for(auto& cell : cells)
    s.append(&cell);
  return s;
}

Selection ProcessModel::selectedChildren() const noexcept
{
  Selection s;
  for(auto& cell : cells)
  {
    if(cell.selection.get())
      s.append(&cell);
  }
  return s;
}

void ProcessModel::setSelection(const Selection& s) const noexcept
{
  for(auto& cell : cells)
    cell.selection.set(s.contains(&cell));
}

} // namespace ClipLauncher

// Serialization
template <>
void DataStreamReader::read(const ClipLauncher::ProcessModel& proc)
{
  readFrom(*proc.inlet);
  readFrom(*proc.outlet);

  m_stream << proc.m_globalQuantization;

  // Lanes
  m_stream << (int32_t)proc.lanes.size();
  for(const auto& lane : proc.lanes)
    readFrom(lane);

  // Scenes
  m_stream << (int32_t)proc.scenes.size();
  for(const auto& scene : proc.scenes)
    readFrom(scene);

  // Cells
  m_stream << (int32_t)proc.cells.size();
  for(const auto& cell : proc.cells)
    readFrom(cell);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(ClipLauncher::ProcessModel& proc)
{
  proc.inlet = Process::load_audio_inlet(*this, &proc);
  proc.outlet = Process::load_audio_outlet(*this, &proc);

  m_stream >> proc.m_globalQuantization;

  // Lanes
  {
    int32_t sz;
    m_stream >> sz;
    for(int i = 0; i < sz; i++)
    {
      auto lane = new ClipLauncher::LaneModel{*this, &proc};
      proc.lanes.add(lane);
    }
  }

  // Scenes
  {
    int32_t sz;
    m_stream >> sz;
    for(int i = 0; i < sz; i++)
    {
      auto scene = new ClipLauncher::SceneModel{*this, &proc};
      proc.scenes.add(scene);
    }
  }

  // Cells
  {
    int32_t cellCount;
    m_stream >> cellCount;
    for(int i = 0; i < cellCount; i++)
    {
      auto cell = new ClipLauncher::CellModel{*this, proc.m_context, &proc};
      proc.cells.add(cell);
    }
  }
}

template <>
void JSONReader::read(const ClipLauncher::ProcessModel& proc)
{
  obj["Inlet"] = *proc.inlet;
  obj["Outlet"] = *proc.outlet;

  obj["GlobalQuantization"] = proc.m_globalQuantization;

  obj["Lanes"] = proc.lanes;
  obj["Scenes"] = proc.scenes;

  // Cells: serialize manually since they need DocumentContext
  obj["Cells"] = proc.cells;
}

template <>
void JSONWriter::write(ClipLauncher::ProcessModel& proc)
{
  if(auto inl = obj.tryGet("Inlet"))
  {
    JSONWriter writer{*inl};
    proc.inlet = Process::load_audio_inlet(writer, &proc);
  }
  else
  {
    proc.inlet = std::make_unique<Process::AudioInlet>(
        "Audio In", Id<Process::Port>(0), &proc);
  }

  if(auto outl = obj.tryGet("Outlet"))
  {
    JSONWriter writer{*outl};
    proc.outlet = Process::load_audio_outlet(writer, &proc);
  }
  else
  {
    proc.outlet = std::make_unique<Process::AudioOutlet>(
        "Audio Out", Id<Process::Port>(0), &proc);
  }

  proc.m_globalQuantization = obj["GlobalQuantization"].toDouble();

  const auto& lanes_arr = obj["Lanes"].toArray();
  for(const auto& val : lanes_arr)
  {
    JSONObject::Deserializer des{val};
    auto lane = new ClipLauncher::LaneModel{des, &proc};
    proc.lanes.add(lane);
  }

  const auto& scenes_arr = obj["Scenes"].toArray();
  for(const auto& val : scenes_arr)
  {
    JSONObject::Deserializer des{val};
    auto scene = new ClipLauncher::SceneModel{des, &proc};
    proc.scenes.add(scene);
  }

  // Cells
  const auto& cells_arr = obj["Cells"].toArray();
  for(const auto& val : cells_arr)
  {
    JSONObject::Deserializer des{val};
    auto cell = new ClipLauncher::CellModel{des, proc.m_context, &proc};
    proc.cells.add(cell);
  }
}
