#pragma once
#include <score_plugin_curve_export.h>
#include <Curve/Commands/SetSegmentParameters.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QPoint>

#include <vector>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
class Model;
class Presenter;
class StateBase;
class SCORE_PLUGIN_CURVE_EXPORT SetSegmentParametersCommandObject
{
public:
  SetSegmentParametersCommandObject(const Model&, const score::CommandStackFacade&);

  void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }

  void press();

  void move();

  void release();

  void cancel();

private:
  const Model& m_model;
  SingleOngoingCommandDispatcher<SetSegmentParameters> m_dispatcher;

  Curve::StateBase* m_state{};
  QPointF m_originalPress;
  ossia::hash_map<
      Id<Curve::SegmentModel>, std::pair<std::optional<double>, std::optional<double>>,
      CurveDataHash>
      m_orig;
  std::vector<std::pair<Id<SegmentModel>, std::pair<double, double>>> m_params;
};
}
