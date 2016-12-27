#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPoint>

#include "FullViewConstraintViewModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;


template <>
void DataStreamReader::read(
    const Scenario::FullViewConstraintViewModel& constraint)
{
  this->read(static_cast<const Scenario::ConstraintViewModel&>(constraint));
  m_stream << constraint.zoom() << constraint.visibleRect();
}


template <>
void JSONObjectReader::readFrom(
    const Scenario::FullViewConstraintViewModel& constraint)
{
  readFrom(static_cast<const Scenario::ConstraintViewModel&>(constraint));
  obj["Zoom"] = constraint.zoom();
  obj["CenterOn"] = toJsonValue(constraint.visibleRect());
}


template <>
void DataStreamWriter::writeTo(
    Scenario::FullViewConstraintViewModel& cvm)
{
  double z;
  QRectF c;
  m_stream >> z >> c;
  cvm.setVisibleRect(c);
  cvm.setZoom(z);
}


template <>
void JSONObjectWriter::writeTo(
    Scenario::FullViewConstraintViewModel& cvm)
{
  auto z = obj["Zoom"].toDouble();
  auto c = obj["CenterOn"].toArray();

  if (c.size() == 4)
    cvm.setVisibleRect(
        QRectF{qreal(c.at(0).toDouble()), qreal(c.at(1).toDouble()),
               qreal(c.at(2).toDouble()), qreal(c.at(3).toDouble())});

  cvm.setZoom(z);
}
