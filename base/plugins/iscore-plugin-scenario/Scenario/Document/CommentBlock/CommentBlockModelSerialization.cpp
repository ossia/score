#include <boost/optional/optional.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include <Process/TimeValue.hpp>
#include <State/Expression.hpp>
#include "CommentBlockModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;

template<>
void Visitor<Reader<DataStream>>::readFrom(const Scenario::CommentBlockModel& comment)
{
    readFrom(static_cast<const IdentifiedObject<Scenario::CommentBlockModel>&>(comment));

    m_stream << comment.m_date
             << comment.m_yposition
             << comment.m_HTMLcontent;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Scenario::CommentBlockModel& comment)
{
    m_stream >> comment.m_date
             >> comment.m_yposition
             >> comment.m_HTMLcontent;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Scenario::CommentBlockModel& comment)
{
    readFrom(static_cast<const IdentifiedObject<Scenario::CommentBlockModel>&>(comment));

    m_obj["Date"] = toJsonValue(comment.m_date);
    m_obj["HeightPercentage"] = comment.m_yposition;
    m_obj["HTMLContent"] = comment.m_HTMLcontent;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Scenario::CommentBlockModel& comment)
{
    comment.m_date = fromJsonValue<TimeValue>(m_obj["Date"]);
    comment.m_yposition = m_obj["HeightPercentage"].toDouble();
    comment.m_HTMLcontent = m_obj["HTMLContent"].toString();
}
