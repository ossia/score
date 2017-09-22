#pragma once
#include <Dataflow/DocumentPlugin.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <score/model/path/Path.hpp>

namespace Dataflow
{
class MoveNode final : public score::Command
{

  SCORE_COMMAND_DECL(Scenario::Command::ScenarioCommandFactoryName(), MoveNode, "Move node")

  public:
    MoveNode(const Process::Node& model, QPointF newpos);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(const Process::Node& m, QPointF pos) { m_new = pos; }
protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Process::Node> m_model;
  QPointF m_old, m_new;
};

class CreateCable final : public score::Command
{
  SCORE_COMMAND_DECL(Scenario::Command::ScenarioCommandFactoryName(), CreateCable, "Create cable")

  public:
    CreateCable(
      const Scenario::ScenarioDocumentModel& dp,
      Id<Process::Cable> theCable, Process::CableData dat);
  CreateCable(
      const Scenario::ScenarioDocumentModel& dp,
      Id<Process::Cable> theCable, const Process::Cable& dat);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Scenario::ScenarioDocumentModel> m_model;
  Id<Process::Cable> m_cable;
  Process::CableData m_dat;
};

class UpdateCable final : public score::Command
{
  SCORE_COMMAND_DECL(Scenario::Command::ScenarioCommandFactoryName(), UpdateCable, "Update cable")

  public:
    UpdateCable(
      Process::Cable& theCable, Process::CableData newDat);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Process::Cable> m_model;
  Process::CableData m_old, m_new;
};

class RemoveCable final : public score::Command
{
  SCORE_COMMAND_DECL(Scenario::Command::ScenarioCommandFactoryName(), RemoveCable, "Remove cable")

  public:
    RemoveCable(
      const Dataflow::DocumentPlugin& dp,
      const Process::Cable& theCable);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Dataflow::DocumentPlugin> m_model;
  Id<Process::Cable> m_cable;
  Process::CableData m_data;
};
}
