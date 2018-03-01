#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <score/model/Identifier.hpp>
#include <ossia/detail/math.hpp>
#include <QDebug>
#include <QPoint>
#include <cmath>
#include <cstddef>
#include <vector>


namespace Curve
{
struct PeriodicSegmentData;
template<typename PeriodicFunction>
class PeriodicSegment;
}


namespace Curve
{
struct PeriodicSegmentData
{
  double freq{5};
  double ampl{0.5};
};

template<typename PeriodicFunction>
class SCORE_PLUGIN_CURVE_EXPORT PeriodicSegment final
    : public SegmentModel
    , public PeriodicSegmentData
{
  static key_type static_concreteKey()
  {
    return Metadata<ConcreteKey_k, PeriodicSegment>::get();
  }
  key_type concreteKey() const final override
  {
    return static_concreteKey();
  }
  void serialize_impl(const VisitorVariant& vis) const final override;

public:
  using data_type = PeriodicSegmentData;
  using SegmentModel::SegmentModel;
  PeriodicSegment(const SegmentData& dat, QObject* parent)
    : SegmentModel{dat, parent}
  {
    const auto& sin_data = dat.specificSegmentData.value<PeriodicSegmentData>();
    freq = sin_data.freq;
    ampl = sin_data.ampl;
  }
  PeriodicSegment(const PeriodicSegment& other, const id_type& id, QObject* parent)
    : SegmentModel{other.start(), other.end(), id, parent}
  {
    freq = other.freq;
    ampl = other.ampl;
  }

  PeriodicSegment(DataStream::Deserializer& vis, QObject* parent)
      : SegmentModel{vis, parent}
  { vis.writeTo(*this); }

  PeriodicSegment(JSONObject::Deserializer& vis, QObject* parent)
      : SegmentModel{vis, parent}
  { vis.writeTo(*this); }


  void on_startChanged() override
  {
    dataChanged();
  }
  void on_endChanged() override
  {
    dataChanged();
  }

  void updateData(int numInterp) const override
  {
    if (std::size_t(2 * numInterp + 1) != m_data.size())
      m_valid = false;
    if (!m_valid)
    {
      numInterp *= 20;
      m_data.resize(numInterp + 1);

      double start_x = start().x();
      double end_x = end().x();
      double start_y = start().y();
      double end_y = end().y();
      for (int j = 0; j <= numInterp; j++)
      {
        QPointF& pt = m_data[j];
        pt.setX(start_x + (double(j) / numInterp) * (end_x - start_x));
        pt.setY((start_y + end_y) / 2. + (end_y - start_y)
                * (ampl * PeriodicFunction{}(ossia::two_pi * freq * double(j) / numInterp)));
      }
    }
  }

  template <typename Y>
  ossia::curve_segment<Y> makeFunction() const
  {
    auto amplitude = ampl;
    auto f = ossia::two_pi * freq;
    return [amplitude,f](double ratio, Y start, Y end) {
      return
          (start + end) / 2. + (end - start)
          * amplitude * PeriodicFunction{}(ratio * f);
    };
  }

  double valueAt(double x) const override
  {
    return (1. + ampl * PeriodicFunction{}(ossia::two_pi * x * freq)) / 2.;
  }

  optional<double> verticalParameter() const override
  {
    return 2. * ampl - 1.;
  }
  optional<double> horizontalParameter() const override
  {
    return (freq - 1.) / 7. - 1.;
  }
  void setVerticalParameter(double p) override
  {
    // From -1; 1 to 0;1
    ampl = (p + 1) / 2.;
    dataChanged();
  }
  void setHorizontalParameter(double p) override
  {
    // From -1; 1 to 1; 15
    freq = (p + 1) * 7 + 1;
    dataChanged();
  }

  QVariant toSegmentSpecificData() const override
  {
    return QVariant::fromValue(PeriodicSegmentData{freq, ampl});
  }

  ossia::curve_segment<float> makeFloatFunction() const override
  {
    return makeFunction<float>();
  }
  ossia::curve_segment<int> makeIntFunction() const override
  {
    return makeFunction<int>();
  }
  ossia::curve_segment<bool> makeBoolFunction() const override
  {
    return makeFunction<bool>();
  }
};

struct Sin
{
    template<typename T>
    auto operator()(T val) { return std::sin(val); }
};
struct Square
{
    template<typename T>
    auto operator()(T val) { return std::sin(val) > 0 ? T(1) : T(-1); }
};
struct Triangle
{
    template<typename T>
    auto operator()(T val) { return std::asin(std::sin(val)); }
};
struct Saw
{
    template<typename T>
    auto operator()(T val) { return std::atan(std::tan(val)); }
};
}

template<typename T>
struct is_custom_serialized<Curve::PeriodicSegment<T>> : public std::true_type {};

template <typename T>
struct TSerializer<DataStream, Curve::PeriodicSegment<T>>
{
    using type =Curve::PeriodicSegment<T>;
    static void readFrom(DataStream::Serializer& s, const type& obj)
    {
    }

    static void writeTo(DataStream::Deserializer& s, type& obj)
    {
    }
};

template <typename T>
struct TSerializer<JSONObject, Curve::PeriodicSegment<T>>
{
    using type =Curve::PeriodicSegment<T>;
    static void readFrom(JSONObject::Serializer& s, const type& obj)
    {
    }

    static void writeTo(JSONObject::Deserializer& s, type& obj)
    {
    }
};

namespace Curve
{
template<typename T>
void PeriodicSegment<T>::serialize_impl(const VisitorVariant& vis) const
{
  if (vis.identifier == DataStream::type())
  {
    TSerializer<DataStream, PeriodicSegment<T>>::readFrom(static_cast<DataStream::Serializer&>(vis.visitor), *this);
    return;
  }
  else if (vis.identifier == JSONObject::type())
  {
    TSerializer<JSONObject, PeriodicSegment<T>>::readFrom(static_cast<JSONObject::Serializer&>(vis.visitor), *this);
    return;
  }

  SCORE_ABORT;
}

}

Q_DECLARE_METATYPE(Curve::PeriodicSegmentData)


CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_CURVE_EXPORT,
    Curve::PeriodicSegment<Curve::Sin>,
    "c16dad6a-a422-4fb7-8bd5-850cbe8c3f28",
    "Sin",
    "Sin",
    "Waveform")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_CURVE_EXPORT,
    Curve::PeriodicSegment<Curve::Square>,
    "09863350-a392-4865-aa28-cac6e5dfe13c",
    "Square",
    "Square",
    "Waveform")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_CURVE_EXPORT,
    Curve::PeriodicSegment<Curve::Triangle>,
    "7053f4fc-691e-42e2-a5f4-6a608a6bd547",
    "Triangle",
    "Triangle",
    "Waveform")
CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_CURVE_EXPORT,
    Curve::PeriodicSegment<Curve::Saw>,
    "23e3a159-b5de-43a3-b965-bf9c48114f02",
    "Saw",
    "Saw",
    "Waveform")
