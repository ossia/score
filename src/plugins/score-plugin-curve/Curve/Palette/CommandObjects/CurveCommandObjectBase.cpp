// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveCommandObjectBase.hpp"

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
CommandObjectBase::CommandObjectBase(Presenter* pres, const score::CommandStackFacade& stack)
    : m_presenter{pres}, m_dispatcher{stack}
{
}

CommandObjectBase::~CommandObjectBase() { }

void CommandObjectBase::press()
{
  const auto& current = m_presenter->model();

  // Serialize the current state of the curve
  m_startSegments = current.toCurveData();

  // To prevent behind locked at 0.000001 or 0.9999
  m_xmin = std::numeric_limits<decltype(m_xmax)>::lowest();
  m_xmax = std::numeric_limits<decltype(m_xmax)>::max();
  m_xLastPoint = -1;

  on_press();
}

void CommandObjectBase::handleLocking()
{
  double current_x = m_state->currentPoint.x();
  double current_y = m_state->currentPoint.y();

  const bool bounded = m_presenter->boundedMove();
  // We lock between O - 1 in both axes.
  if (current_x < 0.)
    m_state->currentPoint.setX(0.);
  else if (bounded && current_x > 1.)
    m_state->currentPoint.setX(1.);

  if (current_y < 0.)
    m_state->currentPoint.setY(0.);
  else if (current_y > 1.)
    m_state->currentPoint.setY(1.);

  // And more specifically...
  if (m_presenter->editionSettings().lockBetweenPoints())
  {
    if (current_x <= m_xmin)
      m_state->currentPoint.setX(m_xmin + 0.000001);

    if (current_x >= m_xmax)
    {
      // If xmax is the max of the whole curve and we are not bounded,
      // we ignore.
      if (!(!bounded && current_x >= m_xLastPoint))
        m_state->currentPoint.setX(m_xmax - 0.000001);
    }
  }
}

void CommandObjectBase::submit(std::vector<SegmentData>&& segments)
{
  m_dispatcher.submit(m_presenter->model(), std::move(segments));
}
}
