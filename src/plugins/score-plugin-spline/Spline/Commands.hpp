#pragma once
#include <score/command/Command.hpp>

#include <ossia/dataflow/nodes/spline.hpp>

namespace Spline
{
class ProcessModel;
const CommandGroupKey& CommandFactoryName();

class ChangeSpline final : public score::Command
{
  SCORE_COMMAND_DECL(Spline::CommandFactoryName(), ChangeSpline, "Change Spline")
public:
  ChangeSpline(const ProcessModel& autom, const ossia::nodes::spline_data& newval);

public:
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(const ProcessModel&, const ossia::nodes::spline_data& newval);
  void update(const ProcessModel&, ossia::nodes::spline_data&& newval);

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_path;
  ossia::nodes::spline_data m_old, m_new;
};
}
