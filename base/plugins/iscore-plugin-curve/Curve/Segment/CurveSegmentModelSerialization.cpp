

#include <iscore/tools/std/StdlibWrapper.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QPoint>
#include <QVariant>
#include <algorithm>
#include <vector>

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentFactoryKey.hpp>
#include "CurveSegmentList.hpp"
#include "CurveSegmentModel.hpp"
#include "CurveSegmentModelSerialization.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>

class QObject;
template <typename T> class IdentifiedObject;
template <typename T> class Reader;
template <typename T> class Writer;
template <typename VisitorType> class Visitor;

template<>
ISCORE_PLUGIN_CURVE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const Curve::SegmentData& segmt)
{
    m_stream << segmt.id
             << segmt.start << segmt.end
             << segmt.previous << segmt.following
             << segmt.type;

    auto& csl = context.components.factory<Curve::SegmentList>();
    auto segmt_fact = csl.list().get(segmt.type);

    ISCORE_ASSERT(segmt_fact);
    segmt_fact->serializeCurveSegmentData(segmt.specificSegmentData, this->toVariant());

    insertDelimiter();
}

template<>
ISCORE_PLUGIN_CURVE_EXPORT void Visitor<Writer<DataStream>>::writeTo(Curve::SegmentData& segmt)
{
    m_stream >> segmt.id
             >> segmt.start >> segmt.end
             >> segmt.previous >> segmt.following
             >> segmt.type;

    auto& csl = context.components.factory<Curve::SegmentList>();
    auto segmt_fact = csl.list().get(segmt.type);
    ISCORE_ASSERT(segmt_fact);
    segmt.specificSegmentData = segmt_fact->makeCurveSegmentData(this->toVariant());

    checkDelimiter();
}

template<>
void Visitor<Reader<DataStream>>::readFrom(
        const Curve::SegmentModel& segmt)
{
    // To allow recration using createProcess
    readFrom(segmt.key());

    // Save the parent class
    readFrom(static_cast<const IdentifiedObject<Curve::SegmentModel>&>(segmt));

    // Save this class (this will be loaded by writeTo(*this) in CurveSegmentModel ctor
    m_stream << segmt.previous() << segmt.following()
             << segmt.start() << segmt.end();

    // Save the subclass
    segmt.serialize(toVariant());

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Curve::SegmentModel& segmt)
{
    m_stream >> segmt.m_previous >> segmt.m_following
             >> segmt.m_start >> segmt.m_end;

    // Note : don't call setStart/setEnd here since they
    // call virtual methods and this may be called from
    // CurveSegmentModel's constructor.

    // Note : delimiter checked in createCurveSegment
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const Curve::SegmentModel& segmt)
{
    using namespace Curve;
    // To allow recration using createProcess
    m_obj["Name"] = toJsonValue(segmt.key());

    // Save the parent class
    readFrom(static_cast<const IdentifiedObject<SegmentModel>&>(segmt));

    // Save this class (this will be loaded by writeTo(*this) in CurveSegmentModel ctor
    m_obj["Previous"] = toJsonValue(segmt.previous());
    m_obj["Following"] = toJsonValue(segmt.following());
    m_obj["Start"] = toJsonValue(segmt.start());
    m_obj["End"] = toJsonValue(segmt.end());

    // Save the subclass
    segmt.serialize(toVariant());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Curve::SegmentModel& segmt)
{
    using namespace Curve;
    segmt.m_previous = fromJsonValue<Id<SegmentModel>>(m_obj["Previous"]);
    segmt.m_following = fromJsonValue<Id<SegmentModel>>(m_obj["Following"]);
    segmt.m_start = fromJsonValue<Curve::Point>(m_obj["Start"]);
    segmt.m_end = fromJsonValue<Curve::Point>(m_obj["End"]);
}


namespace Curve
{
Curve::SegmentModel* createCurveSegment(
        const Curve::SegmentList& csl,
        Deserializer<DataStream>& deserializer,
        QObject* parent)
{
    SegmentFactoryKey name;
    deserializer.writeTo(name);

    auto fact = csl.list().get(name);
    auto model = fact->load(deserializer.toVariant(), parent);

    deserializer.checkDelimiter();
    return model;
}

Curve::SegmentModel* createCurveSegment(
        const Curve::SegmentList& csl,
        Deserializer<JSONObject>& deserializer,
        QObject* parent)
{
    auto fact = csl.list().get(fromJsonValue<SegmentFactoryKey>(deserializer.m_obj["Name"]));
    auto model = fact->load(deserializer.toVariant(), parent);

    return model;
}


Curve::SegmentModel* createCurveSegment(
        const Curve::SegmentList& csl,
        const Curve::SegmentData& dat,
        QObject* parent)
{
    auto fact = csl.list().get(dat.type);
    auto model = fact->load(dat, parent);

    return model;
}
}
