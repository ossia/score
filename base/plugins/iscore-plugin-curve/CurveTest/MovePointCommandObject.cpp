#include "MovePointCommandObject.hpp"
#include "UpdateCurve.hpp"
#include "CurvePresenter.hpp"
#include "CurveModel.hpp"
#include "CurveSegmentModel.hpp"
#include "CurvePointModel.hpp"
#include "CurvePointView.hpp"
#include <iscore/document/DocumentInterface.hpp>

MovePointCommandObject::MovePointCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack):
    CurveCommandObjectBase{presenter},
    m_dispatcher{stack}
{

}

void MovePointCommandObject::on_press()
{
    // Create the command. For now nothing changes.
    m_startSegments.clear();
    auto current = m_presenter->model()->segments();

    std::transform(current.begin(), current.end(), std::back_inserter(m_startSegments),
                   [] (CurveSegmentModel* segment)
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(*segment);
        return arr;
    });
}

#include "CurveSegmentModelSerialization.hpp"
void MovePointCommandObject::move()
{
    // Two cases :
    // Point is on single curve
    // Point is on two curves
    // Where to take the current clicked point ?
    // We have to know it so that we can get the both borders

    // First we deserialize the base segments.
    QVector<CurveSegmentModel*> segments;
    std::transform(m_startSegments.begin(), m_startSegments.end(), std::back_inserter(segments),
                   [] (QByteArray arr)
    {
        Deserializer<DataStream> des(arr);
        return createCurveSegment(des, nullptr);
    });

    qDebug() << "CURERNT POINT " << m_state->currentPoint;
    // Then we look for the point with the correct id.
    auto& pt = static_cast<const CurvePointView*>(m_state->clickedItem)->model();
    if(pt.previous())
    {
        auto prev = *std::find(segments.begin(), segments.end(), pt.previous());
        prev->setEnd(m_state->currentPoint);
    }
    if(pt.following())
    {
        auto foll = *std::find(segments.begin(), segments.end(), pt.following());
        foll->setStart(m_state->currentPoint);
    }


    QVector<QByteArray> newSegments;
    std::transform(segments.begin(), segments.end(), std::back_inserter(newSegments),
                   [] (CurveSegmentModel* segment)
    {
        QByteArray arr;
        Serializer<DataStream> s(&arr);
        s.readFrom(*segment);
        return arr;
    });
    m_dispatcher.submitCommand<UpdateCurve>(iscore::IDocument::path(m_presenter->model()),
                                            std::move(newSegments));
}

void MovePointCommandObject::release()
{
    m_dispatcher.commit();
}

void MovePointCommandObject::cancel()
{
    m_dispatcher.rollback();
}
