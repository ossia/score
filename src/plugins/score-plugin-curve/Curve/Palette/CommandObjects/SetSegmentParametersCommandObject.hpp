#pragma once
#include <Curve/Commands/SetSegmentParameters.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/tools/std/Optional.hpp>

#include <QPoint>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
class Model;
class Presenter;
class StateBase;
class SetSegmentParametersCommandObject
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
  QMap<Id<Curve::SegmentModel>, std::pair<std::optional<double>, std::optional<double>>> m_orig;
};
}
