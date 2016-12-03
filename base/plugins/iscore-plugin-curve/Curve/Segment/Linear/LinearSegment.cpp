#include <QPoint>
#include <iscore/serialization/VisitorCommon.hpp>
#include <vector>

#include "LinearSegment.hpp"
#include <Curve/Palette/CurvePoint.hpp>

#include <iscore/tools/SettableIdentifier.hpp>

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
  emit dataChanged();
}

void LinearSegment::on_endChanged()
{
  emit dataChanged();
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

std::function<float(double, float, float)>
LinearSegment::makeFloatFunction() const
{
  return ossia::curve_segment_linear<float>{};
}

std::function<int(double, int, int)> LinearSegment::makeIntFunction() const
{
  return ossia::curve_segment_linear<int>{};
}

std::function<bool(double, bool, bool)> LinearSegment::makeBoolFunction() const
{
  return ossia::curve_segment_linear<bool>{};
}
}
