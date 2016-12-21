#include <QDebug>
#include <QPoint>
#include <cmath>
#include <cstddef>
#include <iscore/serialization/VisitorCommon.hpp>
#include <vector>

#include "SinSegment.hpp"
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

class QObject;
#include <iscore/model/Identifier.hpp>

namespace Curve
{
SinSegment::SinSegment(const SegmentData& dat, QObject* parent)
    : SegmentModel{dat, parent}
{
  const auto& sin_data = dat.specificSegmentData.value<SinSegmentData>();
  freq = sin_data.freq;
  ampl = sin_data.ampl;
}

SinSegment::SinSegment(
    const SinSegment& other, const id_type& id, QObject* parent)
    : SegmentModel{other.start(), other.end(), id, parent}
    , freq{other.freq}
    , ampl{other.ampl}
{
}

void SinSegment::on_startChanged()
{
  emit dataChanged();
}

void SinSegment::on_endChanged()
{
  emit dataChanged();
}

void SinSegment::updateData(int numInterp) const
{
  if (std::size_t(2 * numInterp + 1) != m_data.size())
    m_valid = false;
  if (!m_valid)
  {
    numInterp *= 2;
    m_data.resize(numInterp + 1);

    double start_x = start().x();
    double end_x = end().x();
    for (int j = 0; j <= numInterp; j++)
    {
      QPointF& pt = m_data[j];
      pt.setX(start_x + (double(j) / numInterp) * (end_x - start_x));
      pt.setY(0.5 + 0.5 * ampl * sin(6.28 * freq * double(j) / numInterp));
    }
  }
}

double SinSegment::valueAt(double x) const
{
  ISCORE_TODO;
  return -1;
}

void SinSegment::setVerticalParameter(double p)
{
  // From -1; 1 to 0;1
  ampl = (p + 1) / 2.;
  emit dataChanged();
}

void SinSegment::setHorizontalParameter(double p)
{
  // From -1; 1 to 1; 15
  freq = (p + 1) * 7 + 1;
  emit dataChanged();
}

optional<double> SinSegment::verticalParameter() const
{
  return 2. * ampl - 1.;
}

optional<double> SinSegment::horizontalParameter() const
{
  return (freq - 1.) / 7. - 1.;
}
}
