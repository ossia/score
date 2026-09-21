#pragma once
#include <Process/Dataflow/Port.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <score/command/AggregateCommand.hpp>

#include <score/model/path/Path.hpp>

#include <span>

#include <utility>

namespace Dataflow
{
class SCORE_PLUGIN_SCENARIO_EXPORT CreateCable final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), CreateCable, "Create cable")

public:
  CreateCable(
      const Scenario::ScenarioDocumentModel& dp, Id<Process::Cable> theCable,
      Process::CableType type, const Process::Port& source, const Process::Port& sink);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Scenario::ScenarioDocumentModel> m_model;
  Id<Process::Cable> m_cable;
  Process::CableData m_dat;
  std::optional<bool> m_previousPropagate{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT UpdateCable final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), UpdateCable, "Update cable")

public:
  UpdateCable(const Process::Cable& theCable, Process::CableType newDat);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Process::Cable> m_model;
  Process::CableType m_old{}, m_new{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT RemoveCable final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), RemoveCable, "Remove cable")

public:
  RemoveCable(const Scenario::ScenarioDocumentModel& dp, const Process::Cable& theCable);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Scenario::ScenarioDocumentModel> m_model;
  Id<Process::Cable> m_cable;
  Process::CableData m_data;
  std::optional<bool> m_previousPropagate{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT ReplaceCable final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(), ReplaceCable, "Replace Cable")
public:
};

//! Orients a pair of ports into {outlet, sink} if they can legally be cabled
//! together; {nullptr, nullptr} otherwise.
SCORE_PLUGIN_SCENARIO_EXPORT
std::pair<const Process::Outlet*, const Process::Inlet*>
getPortsForConnection(const Process::Port& port1, const Process::Port& port2);

SCORE_PLUGIN_SCENARIO_EXPORT
void onCreateCable(
    const score::DocumentContext& ctx, const Process::Port& port1,
    const Process::Port& port2);

SCORE_PLUGIN_SCENARIO_EXPORT
void replaceCable(
    const score::DocumentContext& ctx, const Process::Cable& currentCable,
    const Process::Port& newPort);

//! Moves the end of every cable that can legally go there onto `newPort`, as a
//! single undoable step. Cables that cannot are left untouched.
SCORE_PLUGIN_SCENARIO_EXPORT
void moveCableEndpoints(
    const score::DocumentContext& ctx, std::span<Process::Cable* const> cables,
    const Process::Port& newPort);
}
