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
    const Model& m, const score::CommandStackFacade& stack)
    : m_model{m}
    , m_dispatcher{stack}
{
}

void SetSegmentParametersCommandObject::press()
{
  auto segment = m_state->clickedSegmentId;

  m_orig.clear();
  for(auto& sel : m_model.segments())
  {
    if(sel.selection.get())
    {
      m_orig.insert_or_assign(
          sel.id(), std::pair{sel.verticalParameter(), sel.horizontalParameter()});
    }
  }

  m_originalPress = m_state->currentPoint;
}

void SetSegmentParametersCommandObject::move()
{
  // The clicked segment may be gone: the curve can change between the click
  // and the moment the state machine gets to it.
  const auto& segments = m_model.segments();
  if(segments.find(m_state->clickedSegmentId) == segments.end())
    return;

  const constexpr double amplitude = 2.;
  const double vampl = amplitude * (m_state->currentPoint.y() - m_originalPress.y());
  const double hampl = amplitude * (m_state->currentPoint.x() - m_originalPress.x());
  const auto orig_it = m_orig.find(m_state->clickedSegmentId);
  const auto clicked_orig = orig_it != m_orig.end() ? orig_it->second
                                                    : decltype(orig_it->second){};
  double newVertical
      = clicked_orig.first ? clamp(*clicked_orig.first + vampl, -1., 1.) : 0.;
  double newHorizontal
      = clicked_orig.second ? clamp(*clicked_orig.second + hampl, -1., 1.) : 0.;

  if(qApp->keyboardModifiers() & Qt::ALT)
  {
    // Every selected segment moves by the same amount: sorted, then turned
    // into the flat map in one go.
    m_params.clear();
    m_params.emplace_back(
        m_state->clickedSegmentId, std::pair{newVertical, newHorizontal});
    for(auto& sel : m_model.segments())
    {
      if(!sel.selection.get() || sel.id() == m_state->clickedSegmentId)
        continue;
      if(auto it = m_orig.find(sel.id()); it != m_orig.end())
      {
        const auto& orig = it->second;
        m_params.emplace_back(
            sel.id(),
            std::pair{
                orig.first ? clamp(*orig.first + vampl, -1., 1.) : 0.,
                orig.second ? clamp(*orig.second + hampl, -1., 1.) : 0.});
      }
    }
    std::sort(m_params.begin(), m_params.end(), [](const auto& a, const auto& b) {
      return a.first < b.first;
    });
    m_dispatcher.submit(
        m_model, SegmentParameterMap{
                     boost::container::ordered_unique_range, m_params.begin(),
                     m_params.end()});
  }
  else
  {
    m_dispatcher.submit(
        m_model,
        SegmentParameterMap{{m_state->clickedSegmentId, {newVertical, newHorizontal}}});
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
