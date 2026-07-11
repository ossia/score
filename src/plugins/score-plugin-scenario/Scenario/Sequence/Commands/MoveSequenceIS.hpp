#pragma once
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <Process/TimeValue.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <memory>

namespace Scenario
{
class TimeSyncModel;
class EventModel;
class ProcessModel;
}

namespace Sequence
{
namespace Command
{

class MoveSequenceIS final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), MoveSequenceIS, "Move sequence IS")
public:
  MoveSequenceIS(
      const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId, TimeVal newDate);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(
      const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId, TimeVal newDate);

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<SequenceModel> m_path;
  Id<Scenario::TimeSyncModel> m_tsId;
  TimeVal m_oldDate;
  TimeVal m_newDate;
};

// Ripple move (shift+drag): the IS and everything after it shift in time,
// following sections keep their durations, and the parent interval's end
// event moves by the same delta.
class MoveSequenceISRipple final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), MoveSequenceISRipple,
      "Ripple move sequence IS")
public:
  MoveSequenceISRipple(
      const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId,
      TimeVal newDate);
  ~MoveSequenceISRipple();

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(
      const SequenceModel& seq, const Id<Scenario::TimeSyncModel>& tsId,
      TimeVal newDate);

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<SequenceModel> m_path;
  Id<Scenario::TimeSyncModel> m_tsId{};
  TimeVal m_oldDate{};
  TimeVal m_newDate{};
  TimeVal m_origParentEnd{}; // parent end event date at gesture start
  Id<Scenario::EventModel> m_endEventId{};
  double m_endY{};
  std::unique_ptr<Scenario::Command::MoveEventMeta> m_moveCmd;
};

// Inserts an IS in the middle of a section (rail double-click): the section
// is split at the given date, every automation curve is split there, and the
// value of each parameter at that point is recorded on the new IS state.
class InsertSequenceIS final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), InsertSequenceIS,
      "Insert sequence IS")
public:
  InsertSequenceIS(const SequenceModel& seq, TimeVal date);

  // False when `date` does not fall strictly inside a section.
  bool valid() const noexcept { return m_valid; }

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<SequenceModel> m_path;
  TimeVal m_date{};
  SequenceModel::InsertedIS m_info;
  bool m_valid{};
};

}
}
