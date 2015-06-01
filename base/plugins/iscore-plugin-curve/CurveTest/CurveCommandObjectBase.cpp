#include "CurveCommandObjectBase.hpp"
#include "CurvePresenter.hpp"
#include "CurveModel.hpp"

CurveCommandObjectBase::CurveCommandObjectBase(CurvePresenter* pres):
    m_presenter{pres}
{

}

void CurveCommandObjectBase::press()
{
    // Generally, on Press one would save the previous state of the curve.
    m_originalSegments = m_presenter->model()->segments();
    m_originalPress = m_presenter->pressedPoint();

    on_press();
}
