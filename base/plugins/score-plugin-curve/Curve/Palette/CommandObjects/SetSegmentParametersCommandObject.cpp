// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "SetSegmentParametersCommandObject.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Clamp.hpp>

namespace score
{
class CommandStackFacade;
} // namespace score

namespace Curve
{
SetSegmentParametersCommandObject::SetSegmentParametersCommandObject(
    const Model& m, const score::CommandStackFacade& stack)
    : m_model{m}, m_dispatcher{stack}
{
}

void SetSegmentParametersCommandObject::press()
{
  auto segment = m_state->clickedSegmentId;
  const auto& seg = m_model.segments().at(segment);
  m_verticalOrig = seg.verticalParameter();
  m_horizontalOrig = seg.horizontalParameter();

  m_originalPress = m_state->currentPoint;

  m_dispatcher.submit(
      m_model,
      SegmentParameterMap{{m_state->clickedSegmentId,
                           {m_verticalOrig ? *m_verticalOrig : 0.,
                            m_horizontalOrig ? *m_horizontalOrig : 0.}}});
}

void SetSegmentParametersCommandObject::move()
{
  const double amplitude = 2.;
  double newVertical = m_verticalOrig
                           ? clamp(
                                 *m_verticalOrig
                                     + amplitude
                                           * (m_state->currentPoint.y()
                                              - m_originalPress.y()),
                                 -1., 1.)
                           : 0;
  double newHorizontal = m_horizontalOrig
                             ? clamp(
                                   *m_horizontalOrig
                                       + amplitude
                                             * (m_state->currentPoint.x()
                                                - m_originalPress.x()),
                                   -1., 1.)
                             : 0;

  m_dispatcher.submit(
      m_model, SegmentParameterMap{
                   {m_state->clickedSegmentId, {newVertical, newHorizontal}}});
}

void SetSegmentParametersCommandObject::release()
{
  m_dispatcher.commit();
}

void SetSegmentParametersCommandObject::cancel()
{
  m_dispatcher.rollback();
}
}
