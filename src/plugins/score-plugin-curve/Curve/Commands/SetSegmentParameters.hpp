#pragma once
#include <Curve/Commands/CurveCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Unused.hpp>
#include <score/tools/std/Optional.hpp>

#include <QMap>
#include <QPair>

#include <score_plugin_curve_export.h>

struct DataStreamInput;
struct DataStreamOutput;

namespace Curve
{
class Model;
class SegmentModel;
using SegmentParameterMap = QMap<Id<SegmentModel>, QPair<double, double>>;
class SCORE_PLUGIN_CURVE_EXPORT SetSegmentParameters final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetSegmentParameters, "Set segment parameters")
public:
  SetSegmentParameters(const Model& model, SegmentParameterMap&& parameters);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void update(unused_t, SegmentParameterMap&& segments) { m_new = std::move(segments); }

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Model> m_model;
  SegmentParameterMap m_new;
  QMap<Id<SegmentModel>, QPair<std::optional<double>, std::optional<double>>> m_old;
};
}
