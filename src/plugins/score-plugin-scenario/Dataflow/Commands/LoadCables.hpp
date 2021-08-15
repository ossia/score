#pragma once
#include <Dataflow/Commands/CableHelpers.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

namespace Dataflow
{
class LoadCables final : public score::Command
{
  SCORE_COMMAND_DECL(
      Scenario::Command::CommandFactoryName(),
      LoadCables,
      "Remove cable")

public:
  LoadCables(const ObjectPath& p, const SerializedCables& c)
      : m_cables{c}
  {
    Dataflow::unstripCables(p, m_cables);
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    Dataflow::removeCables(m_cables, ctx);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    Dataflow::restoreCables(m_cables, ctx);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override { s << m_cables; }
  void deserializeImpl(DataStreamOutput& s) override { s >> m_cables; }

private:
  SerializedCables m_cables;
};
}
