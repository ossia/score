#include "CurveCommandObjectBase.hpp"
#include "CurvePresenter.hpp"
#include "CurveModel.hpp"


#include <iscore/serialization/DataStreamVisitor.hpp>
CurveCommandObjectBase::CurveCommandObjectBase(CurvePresenter* pres):
    m_presenter{pres}
{

}

void CurveCommandObjectBase::press()
{
    m_originalPress = m_presenter->pressedPoint();

    on_press();
}
