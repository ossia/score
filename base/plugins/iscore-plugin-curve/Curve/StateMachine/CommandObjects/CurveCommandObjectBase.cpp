#include "CurveCommandObjectBase.hpp"
#include "Curve/CurvePresenter.hpp"
#include "Curve/CurveModel.hpp"


#include <iscore/serialization/DataStreamVisitor.hpp>
CurveCommandObjectBase::CurveCommandObjectBase(CurvePresenter* pres):
    m_presenter{pres}
{

}
// TODO not necessary
void CurveCommandObjectBase::press()
{
    on_press();
}
