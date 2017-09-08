#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

/*
 * Command for vertical move so it does'nt have to resize anything on time axis
 * */
namespace Scenario
{
class IntervalModel;
class ProcessModel;
namespace Command
{
class MoveInterval final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), MoveInterval, "Move a interval")
public:
  MoveInterval(
      const Scenario::ProcessModel& scenar,
      Id<IntervalModel> id,
      double y);

  void update(unused_t, unused_t, double height)
  {
    m_newHeight = height;
  }

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_path;
  Id<IntervalModel> m_interval;
  double m_oldHeight{};
  double m_newHeight{};

  QList<QPair<Id<IntervalModel>, double>> m_selectedIntervals;
};
}
}
