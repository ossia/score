#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include "CurveCommandObjectBase.hpp"
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>

namespace iscore {
class CommandStackFacade;
}  // namespace iscore

CurveCommandObjectBase::CurveCommandObjectBase(
        CurvePresenter* pres,
        iscore::CommandStackFacade& stack):
    m_presenter{pres},
    m_dispatcher{stack},
    m_modelPath{m_presenter->model()}
{

}

CurveCommandObjectBase::~CurveCommandObjectBase()
{

}

void CurveCommandObjectBase::press()
{
    const auto& current = m_presenter->model();

    // Serialize the current state of the curve
    m_startSegments = current.toCurveData();

    // To prevent behind locked at 0.000001 or 0.9999
    m_xmin = -1;
    m_xmax = 2;

    on_press();
}

void CurveCommandObjectBase::handleLocking()
{
    double current_x = m_state->currentPoint.x();
    double current_y = m_state->currentPoint.y();

    // In any case we lock between O - 1 in both axes.
    if(current_x < 0.)
        m_state->currentPoint.setX(0.);
    if(current_x > 1.)
        m_state->currentPoint.setX(1.);
    if(current_y < 0.)
        m_state->currentPoint.setY(0.);
    if(current_y > 1.)
        m_state->currentPoint.setY(1.);

    // And more specifically...
    if(m_presenter->editionSettings().lockBetweenPoints())
    {
        if(current_x <= m_xmin)
            m_state->currentPoint.setX(m_xmin + 0.000001);

        if(current_x >= m_xmax)
            m_state->currentPoint.setX(m_xmax - 0.000001);
    }
}

void CurveCommandObjectBase::submit(std::vector<CurveSegmentData>&& segments)
{
    // TODO std::move
    m_dispatcher.submitCommand(Path<CurveModel>(m_modelPath),
                               std::move(segments));
}


