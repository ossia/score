// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "PowerSegment.hpp"

#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <ossia/editor/curve/curve_segment/linear.hpp>

namespace Curve
{

PowerSegment::PowerSegment(const SegmentData& dat, QObject* parent)
    : SegmentModel{dat, parent}
    , gamma{dat.specificSegmentData.value<PowerSegmentData>().gamma}
{
}

PowerSegment::PowerSegment(
    const PowerSegment& other,
    const IdentifiedObject::id_type& id,
    QObject* parent)
    : SegmentModel{other.start(), other.end(), id, parent}, gamma{other.gamma}
{
}

void PowerSegment::on_startChanged()
{
  dataChanged();
}

void PowerSegment::on_endChanged()
{
  dataChanged();
}

void PowerSegment::updateData(int numInterp) const
{
  if (std::size_t(numInterp + 1) != m_data.size())
    m_valid = false;
  if (!m_valid)
  {
    if (gamma == PowerSegmentData::linearGamma || start() == end()
        || numInterp == 2)
    {
      if (m_data.size() != 2)
        m_data.resize(2);
      m_data[0] = start();
      m_data[1] = end();
    }
    else
    {
      m_data.resize(numInterp + 1);
      double start_x = start().x();
      double start_y = start().y();
      double end_x = end().x();
      double end_y = end().y();

      if (gamma < 1.)
      {
        for (int j = 0; j <= numInterp; j++)
        {
          double pos_x = std::pow(double(j) / numInterp, 3.);
          m_data[numInterp - j]
              = {start_x + pos_x * (end_x - start_x),
                 start_y + std::pow(pos_x, gamma) * (end_y - start_y)};
        }
      }
      else
      {
        for (int j = 0; j <= numInterp; j++)
        {
          double pos_x = 1. - std::pow(double(j) / numInterp, 3.);
          m_data[numInterp - j]
              = {start_x + pos_x * (end_x - start_x),
                 start_y + std::pow(pos_x, gamma) * (end_y - start_y)};
        }
      }
    }
  }
}

double PowerSegment::valueAt(double x) const
{
  if (gamma == PowerSegmentData::linearGamma)
  {
    return start().y()
           + (end().y() - start().y()) * (x - start().x())
                 / (end().x() - start().x());
  }
  else
  {
    return start().y()
           + (end().y() - start().y()) * (std::pow(x, gamma) - start().x())
                 / (end().x() - start().x());
  }
}

void PowerSegment::setVerticalParameter(double p)
{
  if (start().y() < end().y())
    gamma = std::pow(16., -p);
  else
    gamma = std::pow(16., p);

  dataChanged();
}

QVariant PowerSegment::toSegmentSpecificData() const
{
  return QVariant::fromValue(PowerSegmentData(gamma));
}

template <typename Y>
ossia::curve_segment<Y> PowerSegment::makeFunction() const
{
  if (gamma == Curve::PowerSegmentData::linearGamma)
  {
    // We just return the linear one
    return ossia::curve_segment_linear<Y>{};
  }
  else
  {
    double thepow = Curve::PowerSegmentData::linearGamma + 1 - gamma;
    return [=](double ratio, Y start, Y end) {
      return ossia::easing::ease{}(start, end, std::pow(ratio, thepow));
    };
  }
}
ossia::curve_segment<double> PowerSegment::makeDoubleFunction() const
{
  return makeFunction<double>();
}

ossia::curve_segment<float> PowerSegment::makeFloatFunction() const
{
  return makeFunction<float>();
}

ossia::curve_segment<int> PowerSegment::makeIntFunction() const
{
  return makeFunction<int>();
}

optional<double> PowerSegment::verticalParameter() const
{
  if (start().y() < end().y())
  {
    return -std::log(gamma) / std::log(16.);
  }
  else
  {
    return std::log(gamma) / std::log(16.);
  }
}
}

template <>
void DataStreamReader::read(const Curve::PowerSegment& segmt)
{
  m_stream << segmt.gamma;
}

template <>
void DataStreamWriter::write(Curve::PowerSegment& segmt)
{
  m_stream >> segmt.gamma;
}

template <>
void JSONReader::read(const Curve::PowerSegment& segmt)
{
  obj[strings.Power] = segmt.gamma;
}

template <>
void JSONWriter::write(Curve::PowerSegment& segmt)
{
  segmt.gamma = obj[strings.Power].toDouble();
}

template <>
void DataStreamReader::read(const Curve::PowerSegmentData& segmt)
{
  m_stream << segmt.gamma;
}

template <>
void DataStreamWriter::write(Curve::PowerSegmentData& segmt)
{
  m_stream >> segmt.gamma;
}

template <>
void JSONReader::read(const Curve::PowerSegmentData& segmt)
{
  obj[strings.Power] = segmt.gamma;
}

template <>
void JSONWriter::write(Curve::PowerSegmentData& segmt)
{
  segmt.gamma = obj[strings.Power].toDouble();
}
