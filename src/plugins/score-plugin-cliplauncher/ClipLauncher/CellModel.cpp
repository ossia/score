#include "CellModel.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

W_OBJECT_IMPL(ClipLauncher::CellModel)

namespace ClipLauncher
{

CellModel::CellModel(
    const Id<CellModel>& id, const score::DocumentContext& ctx, QObject* parent)
    : score::Entity<CellModel>{id, "Cell", parent}
    , Scenario::BaseScenarioContainer{ctx, this}
{
}

CellModel::CellModel(
    DataStream::Deserializer& vis, const score::DocumentContext& ctx, QObject* parent)
    : score::Entity<CellModel>{vis, parent}
    , Scenario::BaseScenarioContainer{Scenario::BaseScenarioContainer::no_init{}, ctx, this}
{
  vis.writeTo(*this);
}

CellModel::CellModel(
    JSONObject::Deserializer& vis, const score::DocumentContext& ctx, QObject* parent)
    : score::Entity<CellModel>{vis, parent}
    , Scenario::BaseScenarioContainer{Scenario::BaseScenarioContainer::no_init{}, ctx, this}
{
  vis.writeTo(*this);
}

CellModel::~CellModel() { }

void CellModel::setLane(int l)
{
  if(m_lane != l)
  {
    m_lane = l;
    laneChanged(l);
  }
}

void CellModel::setScene(int s)
{
  if(m_scene != s)
  {
    m_scene = s;
    sceneChanged(s);
  }
}

void CellModel::setLaunchMode(LaunchMode m)
{
  if(m_launchMode != m)
  {
    m_launchMode = m;
    launchModeChanged(m);
  }
}

void CellModel::setTriggerStyle(TriggerStyle s)
{
  if(m_triggerStyle != s)
  {
    m_triggerStyle = s;
    triggerStyleChanged(s);
  }
}

void CellModel::setVelocity(double v)
{
  if(m_velocity != v)
  {
    m_velocity = v;
    velocityChanged(v);
  }
}

void CellModel::addTransitionRule(TransitionRule rule)
{
  m_transitionRules.push_back(std::move(rule));
  transitionRulesChanged();
}

void CellModel::removeTransitionRule(int32_t ruleId)
{
  auto it
      = std::find_if(m_transitionRules.begin(), m_transitionRules.end(), [ruleId](auto& r) {
          return r.id == ruleId;
        });
  if(it != m_transitionRules.end())
  {
    m_transitionRules.erase(it);
    transitionRulesChanged();
  }
}

void CellModel::setCellState(CellState s)
{
  if(m_cellState != s)
  {
    m_cellState = s;
    cellStateChanged(s);
  }
}

double CellModel::progress() const noexcept
{
  return interval().duration.playPercentage();
}

void CellModel::setLoopCount(int c)
{
  if(m_loopCount != c)
  {
    m_loopCount = c;
    loopCountChanged(c);
  }
}

} // namespace ClipLauncher

// Serialization
template <>
void DataStreamReader::read(const ClipLauncher::CellModel& cell)
{
  // Serialize the BaseScenarioContainer
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(cell));

  // Serialize cell-specific data
  m_stream << cell.m_lane << cell.m_scene << cell.m_launchMode << cell.m_triggerStyle
           << cell.m_velocity;

  // Transition rules
  m_stream << (int32_t)cell.m_transitionRules.size();
  for(const auto& rule : cell.m_transitionRules)
    readFrom(rule);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(ClipLauncher::CellModel& cell)
{
  // Deserialize the BaseScenarioContainer
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(cell));

  // Deserialize cell-specific data
  m_stream >> cell.m_lane >> cell.m_scene >> cell.m_launchMode >> cell.m_triggerStyle
      >> cell.m_velocity;

  // Transition rules
  int32_t ruleCount;
  m_stream >> ruleCount;
  cell.m_transitionRules.resize(ruleCount);
  for(auto& rule : cell.m_transitionRules)
    writeTo(rule);

  checkDelimiter();
}

template <>
void JSONReader::read(const ClipLauncher::CellModel& cell)
{
  // Serialize the BaseScenarioContainer
  obj["Scenario"] = static_cast<const Scenario::BaseScenarioContainer&>(cell);

  // Cell-specific data
  obj["Lane"] = cell.m_lane;
  obj["Scene"] = cell.m_scene;
  obj["LaunchMode"] = cell.m_launchMode;
  obj["TriggerStyle"] = cell.m_triggerStyle;
  obj["Velocity"] = cell.m_velocity;

  // Transition rules - uses ArraySerializer via TSerializer<JSONObject, std::vector>
  obj["TransitionRules"] = cell.m_transitionRules;
}

template <>
void JSONWriter::write(ClipLauncher::CellModel& cell)
{
  // Deserialize the BaseScenarioContainer
  {
    JSONObject::Deserializer sub{obj["Scenario"]};
    sub.writeTo(static_cast<Scenario::BaseScenarioContainer&>(cell));
  }

  // Cell-specific data
  cell.m_lane = obj["Lane"].toInt();
  cell.m_scene = obj["Scene"].toInt();
  cell.m_launchMode <<= obj["LaunchMode"];
  cell.m_triggerStyle <<= obj["TriggerStyle"];
  cell.m_velocity = obj["Velocity"].toDouble();

  // Transition rules
  cell.m_transitionRules <<= obj["TransitionRules"];
}
