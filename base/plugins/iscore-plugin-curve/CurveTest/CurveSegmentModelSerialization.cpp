#include "CurveSegmentModelSerialization.hpp"
#include <iscore/serialization/VisitorCommon.hpp>
#include "CurveSegmentModel.hpp"
#include "CurveSegmentList.hpp"

// TODO write this process down somewhere
template<>
void Visitor<Reader<DataStream>>::readFrom(const CurveSegmentModel& segmt)
{
    // To allow recration using createProcess
    m_stream << segmt.name();

    // Save the parent class
    readFrom(static_cast<const IdentifiedObject<CurveSegmentModel>&>(segmt));

    // Save this class (this will be loaded by writeTo(*this) in CurveSegmentModel ctor
    m_stream << segmt.previous() << segmt.following()
             << segmt.start() << segmt.end();

    // Save the subclass
    segmt.serialize(toVariant());

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(CurveSegmentModel& segmt)
{
    id_type<CurveSegmentModel> prev, fol;
    QPointF start, end;
    m_stream >> prev >> fol
             >> start >> end;

    segmt.setPrevious(prev);
    segmt.setFollowing(fol);

    segmt.setStart(start);
    segmt.setEnd(end);
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const CurveSegmentModel& segmt)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(CurveSegmentModel& segmt)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}




CurveSegmentModel*createCurveSegment(Deserializer<DataStream>& deserializer, QObject* parent)
{
    QString name;
    deserializer.m_stream >> name;

    auto model = CurveSegmentList::instance()
                 ->get(name)
                 ->load(deserializer.toVariant(), parent);

    deserializer.checkDelimiter();
    return model;
}
