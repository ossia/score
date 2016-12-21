#pragma once
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <QMap>
#include <QPair>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_curve_export.h>

struct DataStreamInput;
struct DataStreamOutput;

namespace Curve
{
class Model;
class SegmentModel;
using SegmentParameterMap = QMap<Id<SegmentModel>, QPair<double, double>>;
class ISCORE_PLUGIN_CURVE_EXPORT SetSegmentParameters final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      CommandFactoryName(), SetSegmentParameters, "Set segment parameters")
public:
  SetSegmentParameters(const Model& model, SegmentParameterMap&& parameters);

  void undo() const override;
  void redo() const override;

  void update(unused_t, SegmentParameterMap&& segments)
  {
    m_new = std::move(segments);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Model> m_model;
  SegmentParameterMap m_new;
  QMap<Id<SegmentModel>, QPair<optional<double>, optional<double>>> m_old;
};
}
