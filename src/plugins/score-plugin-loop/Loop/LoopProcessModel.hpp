#pragma once
#include <Loop/LoopProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>

#include <score/model/Identifier.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <QVector>

#include <score_plugin_loop_export.h>
#include <verdigris>

class DataStream;
class JSONObject;

namespace Scenario
{
class TimeSyncModel;
class IntervalModel;
}

namespace Loop
{
class SCORE_PLUGIN_LOOP_EXPORT ProcessModel final
    : public Process::ProcessModel,
      public Scenario::BaseScenarioContainer
{
  W_OBJECT(ProcessModel)
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Loop::ProcessModel)

public:
  std::unique_ptr<Process::AudioInlet> inlet;
  std::unique_ptr<Process::AudioOutlet> outlet;

  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      const score::DocumentContext& ctx,
      QObject* parentObject);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, const score::DocumentContext& ctx,  QObject* parent)
      : Process::ProcessModel{vis, parent}
      , BaseScenarioContainer{BaseScenarioContainer::no_init{}, ctx, this}
  {
    vis.writeTo(*this);
    init();
  }

  void init();

  using BaseScenarioContainer::event;
  using QObject::event;

  // Process interface
  void startExecution() override;
  void stopExecution() override;
  void reset() override;

  Selection selectableChildren() const noexcept override;
  Selection selectedChildren() const noexcept override;
  void setSelection(const Selection& s) const noexcept override;

  ~ProcessModel() override;
};

SCORE_PLUGIN_LOOP_EXPORT const QVector<Id<Scenario::IntervalModel>>
intervalsBeforeTimeSync(
    const Loop::ProcessModel& scen,
    const Id<Scenario::TimeSyncModel>& timeSyncId);

class LoopIntervalResizer final : public Scenario::IntervalResizer
{
  SCORE_CONCRETE("e4144527-970f-447a-9d6c-fb05c7aebca8")

  bool matches(const Scenario::IntervalModel& m) const noexcept override;
  score::Command* make(
      const Scenario::IntervalModel& itv,
      TimeVal new_duration,
      ExpandMode,
      LockMode) const noexcept override;
  void update(
      score::Command& cmd,
      const Scenario::IntervalModel& interval,
      TimeVal new_duration,
      ExpandMode,
      LockMode) const noexcept override;
};
}
namespace Scenario
{
template <>
struct ElementTraits<Loop::ProcessModel, Scenario::IntervalModel>
{
  static const constexpr auto accessor
      = static_cast<score::IndirectArray<Scenario::IntervalModel, 1> (*)(
          const Scenario::BaseScenarioContainer&)>(&Scenario::intervals);
};
template <>
struct ElementTraits<Loop::ProcessModel, Scenario::EventModel>
{
  static const constexpr auto accessor
      = static_cast<score::IndirectArray<Scenario::EventModel, 2> (*)(
          const Scenario::BaseScenarioContainer&)>(&Scenario::events);
};
template <>
struct ElementTraits<Loop::ProcessModel, Scenario::TimeSyncModel>
{
  static const constexpr auto accessor
      = static_cast<score::IndirectArray<Scenario::TimeSyncModel, 2> (*)(
          const Scenario::BaseScenarioContainer&)>(&Scenario::timeSyncs);
};
template <>
struct ElementTraits<Loop::ProcessModel, Scenario::StateModel>
{
  static const constexpr auto accessor
      = static_cast<score::IndirectArray<Scenario::StateModel, 2> (*)(
          const Scenario::BaseScenarioContainer&)>(&Scenario::states);
};
}
