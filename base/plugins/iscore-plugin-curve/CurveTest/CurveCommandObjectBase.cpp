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
    // Generally, on Press one would save the previous state of the curve.
/*
    for(const auto& segment : m_presenter->model()->segments())
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(*segment);
        m_oldCurveData.append(arr);
    }
*/
    m_originalPress = m_presenter->pressedPoint();

    on_press();
}
