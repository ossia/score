// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DataStreamVisitor.hpp"
#include "JSONValueVisitor.hpp"

#include <score/tools/std/Optional.hpp>

#include <boost/none_t.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPoint>
#include <QTransform>

#include <array>

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#include <QCborValue>
#endif

static auto makeJsonArray(std::initializer_list<QJsonValue> lst)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
  QJsonArray arr;
  for (auto val : lst)
  {
    arr.append(val);
  }
  return arr;
#else
  return QJsonArray(lst);
#endif
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueReader::read(const QPointF& pt)
{
  val = makeJsonArray({pt.x(), pt.y()});
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueReader::read(const QSizeF& pt)
{
  val = makeJsonArray({pt.width(), pt.height()});
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueReader::read(const QColor& p)
{
  auto pt = p.toRgb();
  val = makeJsonArray({pt.red(), pt.green(), pt.blue(), pt.alpha()});
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueWriter::write(QColor& pt)
{
  auto arr = val.toArray();
  pt = QColor{arr[0].toInt(), arr[1].toInt(), arr[2].toInt(), arr[3].toInt()};
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueWriter::write(QPointF& pt)
{
  const auto arr = val.toArray();
  pt.setX(arr[0].toDouble());
  pt.setY(arr[1].toDouble());
}
template <>
SCORE_LIB_BASE_EXPORT void JSONValueWriter::write(QSizeF& pt)
{
  const auto arr = val.toArray();
  pt.setWidth(arr[0].toDouble());
  pt.setHeight(arr[1].toDouble());
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueReader::read(const QRectF& pt)
{
  val = makeJsonArray({pt.x(), pt.y(), pt.width(), pt.height()});
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueWriter::write(QRectF& pt)
{
  auto arr = val.toArray();
  pt.setX(arr[0].toDouble());
  pt.setY(arr[1].toDouble());
  pt.setWidth(arr[2].toDouble());
  pt.setHeight(arr[3].toDouble());
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueReader::read(const QTransform& pt)
{
  val = makeJsonArray({pt.m11(),
                       pt.m12(),
                       pt.m13(),
                       pt.m21(),
                       pt.m22(),
                       pt.m23(),
                       pt.m31(),
                       pt.m32(),
                       pt.m33()});
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueWriter::write(QTransform& pt)
{
  auto arr = val.toArray();
  pt.setMatrix(
      arr[0].toDouble(),
      arr[1].toDouble(),
      arr[2].toDouble(),
      arr[3].toDouble(),
      arr[4].toDouble(),
      arr[5].toDouble(),
      arr[6].toDouble(),
      arr[7].toDouble(),
      arr[8].toDouble());
}

template <>
SCORE_LIB_BASE_EXPORT void
DataStreamReader::read(const std::array<float, 2>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream << obj[i];
  insertDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(std::array<float, 2>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream >> obj[i];
  checkDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void
DataStreamReader::read(const std::array<float, 3>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream << obj[i];
  insertDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(std::array<float, 3>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream >> obj[i];
  checkDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void
DataStreamReader::read(const std::array<float, 4>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream << obj[i];
  insertDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(std::array<float, 4>& obj)
{
  for (auto i = 0U; i < obj.size(); i++)
    m_stream >> obj[i];
  checkDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const QJsonObject& obj)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
  QJsonDocument doc{obj};
  m_stream << doc.toBinaryData();
#else
  m_stream << QCborValue::fromJsonValue(obj);
#endif
  insertDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(QJsonObject& obj)
{
  QByteArray arr;
  m_stream >> arr;

  obj = QJsonDocument::fromBinaryData(arr).object();

  checkDelimiter();
}
