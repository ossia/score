#pragma once
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Unused.hpp>

#include <vector>

namespace Curve
{
class Model;

class SCORE_PLUGIN_CURVE_EXPORT UpdateCurve final
    : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), UpdateCurve, "Update Curve")
public:
  UpdateCurve(const Model& model, std::vector<SegmentData>&& segments);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(unused_t, std::vector<SegmentData>&& segments)
  {
    m_newCurveData = std::move(segments);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Model> m_model;
  std::vector<SegmentData> m_oldCurveData;
  std::vector<SegmentData> m_newCurveData;
};
}
