// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QPoint>
#include <score/serialization/VisitorCommon.hpp>
#include <vector>

#include "LinearSegment.hpp"
#include <Curve/Palette/CurvePoint.hpp>

#include <score/model/Identifier.hpp>

#include <ossia/editor/curve/curve_segment/linear.hpp>

namespace Curve
{
LinearSegment::LinearSegment(
    const LinearSegment& other,
    const IdentifiedObject::id_type& id,
    QObject* parent)
    : SegmentModel{other.start(), other.end(), id, parent}
{
}

void LinearSegment::on_startChanged()
{
  dataChanged();
}

void LinearSegment::on_endChanged()
{
  dataChanged();
}

void LinearSegment::updateData(int numInterp) const
{
  if (!m_valid)
  {
    if (m_data.size() != 2)
      m_data.resize(2);
    m_data[0] = start();
    m_data[1] = end();
  }
}

double LinearSegment::valueAt(double x) const
{
  return start().y()
         + (end().y() - start().y()) * (x - start().x())
               / (end().x() - start().x());
}

QVariant LinearSegment::toSegmentSpecificData() const
{
  return QVariant::fromValue(data_type{});
}

ossia::curve_segment<float> LinearSegment::makeFloatFunction() const
{
  return ossia::curve_segment_linear<float>{};
}

ossia::curve_segment<int> LinearSegment::makeIntFunction() const
{
  return ossia::curve_segment_linear<int>{};
}

ossia::curve_segment<bool> LinearSegment::makeBoolFunction() const
{
  return ossia::curve_segment_linear<bool>{};
}
}
