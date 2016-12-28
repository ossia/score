#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>
#include <algorithm>
#include <iscore/tools/std/Optional.hpp>

#include "CommentBlockModel.hpp"
#include <Process/TimeValue.hpp>
#include <State/Expression.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/Identifier.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;


template <>
void DataStreamReader::read(
    const Scenario::CommentBlockModel& comment)
{
  m_stream << comment.m_date << comment.m_yposition << comment.m_HTMLcontent;

  insertDelimiter();
}


template <>
void DataStreamWriter::write(Scenario::CommentBlockModel& comment)
{
  m_stream >> comment.m_date >> comment.m_yposition >> comment.m_HTMLcontent;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Scenario::CommentBlockModel& comment)
{
  obj["Date"] = toJsonValue(comment.m_date);
  obj["HeightPercentage"] = comment.m_yposition;
  obj["HTMLContent"] = comment.m_HTMLcontent;
}


template <>
void JSONObjectWriter::write(Scenario::CommentBlockModel& comment)
{
  comment.m_date = fromJsonValue<TimeValue>(obj["Date"]);
  comment.m_yposition = obj["HeightPercentage"].toDouble();
  comment.m_HTMLcontent = obj["HTMLContent"].toString();
}
