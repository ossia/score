#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>

namespace Curve
{
class SinSegment;
}

CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_CURVE_EXPORT,
    Curve::SinSegment,
    "c16dad6a-a422-4fb7-8bd5-850cbe8c3f28",
    "Sin",
    "Sin")

namespace Curve
{
struct SinSegmentData
{
  double freq;
  double ampl;
};

class ISCORE_PLUGIN_CURVE_EXPORT SinSegment final : public SegmentModel
{
  MODEL_METADATA_IMPL(SinSegment)
public:
  using data_type = SinSegmentData;
  using SegmentModel::SegmentModel;
  SinSegment(const SegmentData& dat, QObject* parent);
  SinSegment(const SinSegment& other, const id_type& id, QObject* parent);

  template <typename Impl>
  SinSegment(Deserializer<Impl>& vis, QObject* parent)
      : SegmentModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  void on_startChanged() override;
  void on_endChanged() override;

  void updateData(int numInterp) const override;
  double valueAt(double x) const override;

  optional<double> verticalParameter() const override;
  optional<double> horizontalParameter() const override;
  void setVerticalParameter(double p) override;
  void setHorizontalParameter(double p) override;

  double freq = 5;
  double ampl = 0.6;

  QVariant toSegmentSpecificData() const override
  {
    return QVariant::fromValue(SinSegmentData{freq, ampl});
  }

  template <typename Y>
  std::function<Y(double, Y, Y)> makeFunction() const
  {
    return [=](double ratio, Y start, Y end) {
      return (end - start) / 2.
             + ampl * std::sin(6.28 * ratio / freq) * (end - start) / 2.;
    };
  }
  std::function<float(double, float, float)> makeFloatFunction() const override
  {
    return makeFunction<float>();
  }
  std::function<int(double, int, int)> makeIntFunction() const override
  {
    return makeFunction<int>();
  }
  std::function<bool(double, bool, bool)> makeBoolFunction() const override
  {
    return makeFunction<bool>();
  }
};
}

Q_DECLARE_METATYPE(Curve::SinSegmentData)
