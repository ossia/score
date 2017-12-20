#pragma once
#include <Loop/LoopProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <QByteArray>
#include <QString>
#include <QVector>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>

#include <Process/TimeValue.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score_plugin_loop_export.h>

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
  Q_OBJECT
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Loop::ProcessModel)

public:
  std::unique_ptr<Process::Inlet> inlet;
  std::unique_ptr<Process::Outlet> outlet;
  Process::Inlets inlets() const override;
  Process::Outlets outlets() const override;

  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parentObject);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
      , BaseScenarioContainer{BaseScenarioContainer::no_init{}, this}
  {
    vis.writeTo(*this);
  }

  using BaseScenarioContainer::event;
  using QObject::event;

  // Process interface
  void startExecution() override;
  void stopExecution() override;
  void reset() override;

  Selection selectableChildren() const override;
  Selection selectedChildren() const override;
  void setSelection(const Selection& s) const override;

  ~ProcessModel();
};

SCORE_PLUGIN_LOOP_EXPORT const QVector<Id<Scenario::IntervalModel>>
intervalsBeforeTimeSync(
    const Loop::ProcessModel& scen,
    const Id<Scenario::TimeSyncModel>& timeSyncId);
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
