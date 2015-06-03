#include "MovePointCommandObject.hpp"
#include "UpdateCurve.hpp"
#include "CurvePresenter.hpp"
#include "CurveModel.hpp"
#include "CurveSegmentModel.hpp"
#include <iscore/document/DocumentInterface.hpp>

MovePointCommandObject::MovePointCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack):
    CurveCommandObjectBase{presenter},
    m_dispatcher{stack}
{

}

void MovePointCommandObject::on_press()
{
    // Create the command. For now nothing changes.
    QVector<QByteArray> newSegments;
    auto current = m_presenter->model()->segments();

    std::transform(current.begin(), current.end(), std::back_inserter(newSegments),
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

void MovePointCommandObject::move()
{
    // Two cases :
    // Point is on single curve
    // Point is on two curves
    // Where to take the current clicked point ?
    // We have to know it so that we can get the both borders
    QVector<QByteArray> newSegments;
    auto current = m_presenter->model()->segments();

    std::transform(current.begin(), current.end(), std::back_inserter(newSegments),
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
