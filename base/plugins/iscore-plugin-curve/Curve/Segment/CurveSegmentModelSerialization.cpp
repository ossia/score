#include "CurveSegmentModelSerialization.hpp"
#include <iscore/serialization/VisitorCommon.hpp>
#include "CurveSegmentModel.hpp"
#include "CurveSegmentList.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const CurveSegmentData& segmt)
{
    m_stream << segmt.id
             << segmt.start << segmt.end
             << segmt.previous << segmt.following;

    auto segmt_fact = SingletonCurveSegmentList::instance().get(segmt.type);
    ISCORE_ASSERT(segmt_fact);
    segmt_fact->serializeCurveSegmentData(segmt.specificSegmentData, this->toVariant());

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(CurveSegmentData& segmt)
{
    m_stream >> segmt.id
             >> segmt.start >> segmt.end
             >> segmt.previous >> segmt.following;

    auto segmt_fact = SingletonCurveSegmentList::instance().get(segmt.type);
    ISCORE_ASSERT(segmt_fact);
    segmt.specificSegmentData = segmt_fact->makeCurveSegmentData(this->toVariant());

    checkDelimiter();
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const std::vector<CurveSegmentData>& segmt)
{
    m_stream << (int)segmt.size();
    for(const auto& elt : segmt)
        readFrom(elt);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(std::vector<CurveSegmentData>& segmt)
{
    int n = 0;
    m_stream >> n;

    // TODO see if this could be used as a basis for generic tools for
    // serialization of template elements,
    // by providing a VisitorTemplateImpl<std::vector> that also has readfrom / writeto
    // and could be used for different types..
    segmt.clear();
    segmt.resize(n);
    for(int i = 0; i < n; i++)
    {
        writeTo(segmt[i]);
    }

    checkDelimiter();
}

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
    m_stream >> segmt.m_previous >> segmt.m_following
             >> segmt.m_start >> segmt.m_end;

    // Note : don't call setStart/setEnd here since they
    // call virtual methods and this may be called from
    // CurveSegmentModel's constructor.

    // Note : delimiter checked in createCurveSegment
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const CurveSegmentModel& segmt)
{
    // To allow recration using createProcess
    m_obj["Name"] = segmt.name();

    // Save the parent class
    readFrom(static_cast<const IdentifiedObject<CurveSegmentModel>&>(segmt));

    // Save this class (this will be loaded by writeTo(*this) in CurveSegmentModel ctor
    m_obj["Previous"] = toJsonValue(segmt.previous());
    m_obj["Following"] = toJsonValue(segmt.following());
    m_obj["Start"] = toJsonValue(segmt.start());
    m_obj["End"] = toJsonValue(segmt.end());

    // Save the subclass
    segmt.serialize(toVariant());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(CurveSegmentModel& segmt)
{
    segmt.m_previous = fromJsonValue<Id<CurveSegmentModel>>(m_obj["Previous"]);
    segmt.m_following = fromJsonValue<Id<CurveSegmentModel>>(m_obj["Following"]);
    segmt.m_start = fromJsonValue<CurvePoint>(m_obj["Start"]);
    segmt.m_end = fromJsonValue<CurvePoint>(m_obj["End"]);
}




CurveSegmentModel*createCurveSegment(
        Deserializer<DataStream>& deserializer,
        QObject* parent)
{
    QString name;
    deserializer.m_stream >> name;

    auto& instance = SingletonCurveSegmentList::instance();
    auto fact = instance.get(name);
    auto model = fact->load(deserializer.toVariant(), parent);

    deserializer.checkDelimiter();
    return model;
}

CurveSegmentModel*createCurveSegment(
        Deserializer<JSONObject>& deserializer,
        QObject* parent)
{
    auto& instance = SingletonCurveSegmentList::instance();
    auto fact = instance.get(deserializer.m_obj["Name"].toString());
    auto model = fact->load(deserializer.toVariant(), parent);

    return model;
}


CurveSegmentModel*createCurveSegment(const CurveSegmentData& dat, QObject* parent)
{
    auto& instance = SingletonCurveSegmentList::instance();
    auto fact = instance.get(dat.type);
    auto model = fact->load(dat, parent);

    return model;

}
