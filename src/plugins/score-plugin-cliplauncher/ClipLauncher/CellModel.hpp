#pragma once
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <score/model/Entity.hpp>
#include <score/selection/Selectable.hpp>

#include <ClipLauncher/TransitionRule.hpp>
#include <ClipLauncher/Types.hpp>

#include <score_plugin_cliplauncher_export.h>

#include <verdigris>

namespace ClipLauncher
{

// CellModel inherits from BaseScenarioContainer (like BaseScenario does)
// so that interval.parent() can be dynamic_cast'd to ScenarioInterface*.
class CellModel final
    : public score::Entity<CellModel>
    , public Scenario::BaseScenarioContainer
{
  W_OBJECT(CellModel)
  SCORE_SERIALIZE_FRIENDS

public:
  CellModel(
      const Id<CellModel>& id, const score::DocumentContext& ctx, QObject* parent);
  CellModel(
      DataStream::Deserializer& vis, const score::DocumentContext& ctx, QObject* parent);
  CellModel(
      JSONObject::Deserializer& vis, const score::DocumentContext& ctx, QObject* parent);

  ~CellModel() override;

  Selectable selection{this};

  // Access the BaseScenarioContainer part
  Scenario::BaseScenarioContainer& scenarioContainer() noexcept { return *this; }
  const Scenario::BaseScenarioContainer& scenarioContainer() const noexcept
  {
    return *this;
  }

  // Convenience: the interval holding processes for this cell
  using Scenario::BaseScenarioContainer::interval;

  // Grid position
  int lane() const noexcept { return m_lane; }
  void setLane(int l);
  int scene() const noexcept { return m_scene; }
  void setScene(int s);

  // Cell-specific properties
  LaunchMode launchMode() const noexcept { return m_launchMode; }
  void setLaunchMode(LaunchMode m);

  TriggerStyle triggerStyle() const noexcept { return m_triggerStyle; }
  void setTriggerStyle(TriggerStyle s);

  double velocity() const noexcept { return m_velocity; }
  void setVelocity(double v);

  // Transition rules
  const std::vector<TransitionRule>& transitionRules() const noexcept
  {
    return m_transitionRules;
  }
  void addTransitionRule(TransitionRule rule);
  void removeTransitionRule(int32_t ruleId);

  // Runtime state (not serialized)
  CellState cellState() const noexcept { return m_cellState; }
  void setCellState(CellState s);
  double progress() const noexcept;
  int loopCount() const noexcept { return m_loopCount; }
  void setLoopCount(int c);

  // Signals
  void laneChanged(int l) E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, laneChanged, l)
  void sceneChanged(int s) E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, sceneChanged, s)
  void launchModeChanged(ClipLauncher::LaunchMode m)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, launchModeChanged, m)
  void triggerStyleChanged(ClipLauncher::TriggerStyle s)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, triggerStyleChanged, s)
  void velocityChanged(double v)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, velocityChanged, v)
  void cellStateChanged(ClipLauncher::CellState s)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, cellStateChanged, s)
  void loopCountChanged(int c)
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, loopCountChanged, c)
  void transitionRulesChanged()
      E_SIGNAL(SCORE_PLUGIN_CLIPLAUNCHER_EXPORT, transitionRulesChanged)

private:
  int m_lane{0};
  int m_scene{0};
  LaunchMode m_launchMode{LaunchMode::Immediate};
  TriggerStyle m_triggerStyle{TriggerStyle::Trigger};
  double m_velocity{1.0};

  std::vector<TransitionRule> m_transitionRules;

  // Runtime state (not serialized)
  CellState m_cellState{CellState::Stopped};
  int m_loopCount{0};
};

// intervalsBeforeTimeSync overload for CellModel (needed by AddTrigger/RemoveTrigger templates)
inline const QVector<Id<Scenario::IntervalModel>>
intervalsBeforeTimeSync(const CellModel& cell, const Id<Scenario::TimeSyncModel>& timeSyncId)
{
  if(timeSyncId == cell.endTimeSync().id())
    return {cell.interval().id()};
  return {};
}

} // namespace ClipLauncher

DEFAULT_MODEL_METADATA(ClipLauncher::CellModel, "Cell")
UNDO_NAME_METADATA(EMPTY_MACRO, ClipLauncher::CellModel, "Cell")
