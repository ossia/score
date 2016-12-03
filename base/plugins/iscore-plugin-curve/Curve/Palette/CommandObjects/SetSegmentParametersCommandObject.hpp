#pragma once
#include <QPoint>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <Curve/Commands/SetSegmentParameters.hpp>

namespace iscore
{
class CommandStackFacade;
} // namespace iscore

namespace Curve
{
class Model;
class Presenter;
class StateBase;
class SetSegmentParametersCommandObject
{
public:
  SetSegmentParametersCommandObject(
      const Model&, const iscore::CommandStackFacade&);

  void setCurveState(Curve::StateBase* stateBase)
  {
    m_state = stateBase;
  }

  void press();

  void move();

  void release();

  void cancel();

private:
  const Model& m_model;
  SingleOngoingCommandDispatcher<SetSegmentParameters> m_dispatcher;

  Curve::StateBase* m_state{};
  QPointF m_originalPress;
  optional<double> m_verticalOrig, m_horizontalOrig;
};
}
