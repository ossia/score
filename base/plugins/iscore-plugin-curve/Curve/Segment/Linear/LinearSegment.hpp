#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>

namespace Curve
{
class LinearSegment;
}

CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_CURVE_EXPORT,
    Curve::LinearSegment,
    "a8bd14e2-d7e4-47cd-b76a-6a88fa11f0d2",
    "Linear",
    "Linear")

namespace Curve
{
struct ISCORE_PLUGIN_CURVE_EXPORT LinearSegmentData
{
};

class ISCORE_PLUGIN_CURVE_EXPORT LinearSegment final : public SegmentModel
{
  MODEL_METADATA_IMPL(LinearSegment)
public:
  using data_type = LinearSegmentData;
  using SegmentModel::SegmentModel;

  LinearSegment(
      const LinearSegment& other, const id_type& id, QObject* parent);

  template <typename Impl>
  LinearSegment(Impl& vis, QObject* parent)
      : SegmentModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  void on_startChanged() override;
  void on_endChanged() override;

  void updateData(int numInterp) const override;
  double valueAt(double x) const override;

  QVariant toSegmentSpecificData() const override;

  std::function<float(double, float, float)>
  makeFloatFunction() const override;
  std::function<int(double, int, int)> makeIntFunction() const override;
  std::function<bool(double, bool, bool)> makeBoolFunction() const override;
};
}

Q_DECLARE_METATYPE(Curve::LinearSegmentData)
