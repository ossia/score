// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CommentBlockModel.hpp"

#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <State/Expression.hpp>

#include <score/model/Identifier.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;

template <>
void DataStreamReader::read(const Scenario::CommentBlockModel& comment)
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
void JSONReader::read(const Scenario::CommentBlockModel& comment)
{
  obj["Date"] = comment.m_date;
  obj["HeightPercentage"] = comment.m_yposition;
  obj["HTMLContent"] = comment.m_HTMLcontent;
}

template <>
void JSONWriter::write(Scenario::CommentBlockModel& comment)
{
  comment.m_date <<= obj["Date"];
  comment.m_yposition = obj["HeightPercentage"].toDouble();
  comment.m_HTMLcontent = obj["HTMLContent"].toString();
}
