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
    on_press();
}
