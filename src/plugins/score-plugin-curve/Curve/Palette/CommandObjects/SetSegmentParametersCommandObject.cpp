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

#include <QGuiApplication>

namespace Curve
{
SetSegmentParametersCommandObject::SetSegmentParametersCommandObject(
    const Model& m,
    const score::CommandStackFacade& stack)
    : m_model{m}, m_dispatcher{stack}
{
}

void SetSegmentParametersCommandObject::press()
{
  auto segment = m_state->clickedSegmentId;

  for (auto& sel : m_model.segments())
  {
    if (sel.selection.get())
    {
      m_orig[sel.id()] = {sel.verticalParameter(), sel.horizontalParameter()};
    }
  }

  m_originalPress = m_state->currentPoint;
}

void SetSegmentParametersCommandObject::move()
{
  const constexpr double amplitude = 2.;
  const double vampl = amplitude * (m_state->currentPoint.y() - m_originalPress.y());
  const double hampl = amplitude * (m_state->currentPoint.x() - m_originalPress.x());
  auto clicked_orig = m_orig[m_state->clickedSegmentId];
  double newVertical = clicked_orig.first ? clamp(*clicked_orig.first + vampl, -1., 1.) : 0.;
  double newHorizontal = clicked_orig.second ? clamp(*clicked_orig.second + hampl, -1., 1.) : 0.;

  if (qApp->keyboardModifiers() & Qt::ALT)
  {
    SegmentParameterMap map{{m_state->clickedSegmentId, {newVertical, newHorizontal}}};

    for (auto& sel : m_model.segments())
    {
      if (sel.selection.get())
      {
        auto& orig = m_orig[sel.id()];
        auto& newp = map[sel.id()];
        newp.first = orig.first ? clamp(*orig.first + vampl, -1., 1.) : 0.;
        newp.second = orig.second ? clamp(*orig.second + hampl, -1., 1.) : 0.;
      }
    }
    m_dispatcher.submit(m_model, std::move(map));
  }
  else
  {
    m_dispatcher.submit(
        m_model, SegmentParameterMap{{m_state->clickedSegmentId, {newVertical, newHorizontal}}});
  }
}

void SetSegmentParametersCommandObject::release()
{
  m_dispatcher.commit();
  m_orig.clear();
}

void SetSegmentParametersCommandObject::cancel()
{
  m_dispatcher.rollback();
  m_orig.clear();
}
}
