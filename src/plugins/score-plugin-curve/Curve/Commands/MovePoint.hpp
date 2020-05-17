#pragma once
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <Curve/Palette/CurvePoint.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <score_plugin_curve_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Curve
{
class Model;
class PointModel;
class SCORE_PLUGIN_CURVE_EXPORT MovePoint final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MovePoint, "Move a point")
public:
  MovePoint(const Model& curve, const Id<PointModel>& pointId, Curve::Point newPoint);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(const Model& obj, const Id<PointModel>& pointId, const Curve::Point& newPoint);

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Model> m_model;
  Id<PointModel> m_pointId;
  Curve::Point m_newPoint;
  Curve::Point m_oldPoint;
};
}
