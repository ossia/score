#include "SetSegmentParametersCommandObject.hpp"
#include "Curve/CurveModel.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/Segment/CurveSegmentModel.hpp"
#include <iscore/tools/Clamp.hpp>
#include <iscore/document/DocumentInterface.hpp>

SetSegmentParametersCommandObject::SetSegmentParametersCommandObject(
        CurvePresenter *pres,
        iscore::CommandStack & stack):
    m_presenter{pres},
    m_dispatcher{stack}
{

}

void SetSegmentParametersCommandObject::press()
{
    auto segment = m_state->clickedSegmentId;
    const auto& seg = m_presenter->model().segments().at(segment);
    m_verticalOrig = seg.verticalParameter();
    m_horizontalOrig = seg.horizontalParameter();

    m_originalPress = m_state->currentPoint;

    m_dispatcher.submitCommand(
                iscore::IDocument::path(m_presenter->model()),
    SegmentParameterMap{{m_state->clickedSegmentId, {m_verticalOrig.get_value_or(0), m_horizontalOrig.get_value_or(0)}}});

}

void SetSegmentParametersCommandObject::move()
{
     double newVertical = m_verticalOrig
      ? clamp(*m_verticalOrig + (m_state->currentPoint.y() - m_originalPress.y()), -1., 1.)
      : 0;
    double newHorizontal = m_horizontalOrig
      ? clamp(*m_horizontalOrig + (m_state->currentPoint.x() - m_originalPress.x()) , -1., 1.)
      : 0;

    // OPTIMIZEME
    m_dispatcher.submitCommand(
        Path<CurveModel>{},
        SegmentParameterMap{{m_state->clickedSegmentId, {newVertical, newHorizontal
          }}});
}

void SetSegmentParametersCommandObject::release()
{
    m_dispatcher.commit();
}

void SetSegmentParametersCommandObject::cancel()
{
    m_dispatcher.rollback();
}
