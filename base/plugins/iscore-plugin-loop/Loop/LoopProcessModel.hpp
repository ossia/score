#pragma once
#include <Loop/LoopProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <QByteArray>
#include <QString>
#include <QVector>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>

#include <Process/TimeValue.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_loop_export.h>

class DataStream;
class JSONObject;

namespace Scenario
{
class TimeNodeModel;
class ConstraintModel;
}

namespace Loop
{
class ISCORE_PLUGIN_LOOP_EXPORT ProcessModel final
    : public Process::ProcessModel,
      public Scenario::BaseScenarioContainer
{
  ISCORE_SERIALIZE_FRIENDS
  MODEL_METADATA_IMPL(Loop::ProcessModel)

public:
  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parentObject);

  explicit ProcessModel(
      const Loop::ProcessModel& source,
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

ISCORE_PLUGIN_LOOP_EXPORT const QVector<Id<Scenario::ConstraintModel>>
constraintsBeforeTimeNode(
    const Loop::ProcessModel& scen,
    const Id<Scenario::TimeNodeModel>& timeNodeId);
}
namespace Scenario
{
template <>
struct ElementTraits<Loop::ProcessModel, Scenario::ConstraintModel>
{
  static const constexpr auto accessor
      = static_cast<iscore::IndirectArray<Scenario::ConstraintModel, 1> (*)(
          const Scenario::BaseScenarioContainer&)>(&Scenario::constraints);
};
template <>
struct ElementTraits<Loop::ProcessModel, Scenario::EventModel>
{
  static const constexpr auto accessor
      = static_cast<iscore::IndirectArray<Scenario::EventModel, 2> (*)(
          const Scenario::BaseScenarioContainer&)>(&Scenario::events);
};
template <>
struct ElementTraits<Loop::ProcessModel, Scenario::TimeNodeModel>
{
  static const constexpr auto accessor
      = static_cast<iscore::IndirectArray<Scenario::TimeNodeModel, 2> (*)(
          const Scenario::BaseScenarioContainer&)>(&Scenario::timeNodes);
};
template <>
struct ElementTraits<Loop::ProcessModel, Scenario::StateModel>
{
  static const constexpr auto accessor
      = static_cast<iscore::IndirectArray<Scenario::StateModel, 2> (*)(
          const Scenario::BaseScenarioContainer&)>(&Scenario::states);
};
}
