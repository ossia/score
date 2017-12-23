#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>

namespace Curve
{
class LinearSegment;
}

CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_CURVE_EXPORT,
    Curve::LinearSegment,
    "a8bd14e2-d7e4-47cd-b76a-6a88fa11f0d2",
    "Linear",
    "Linear", "")

namespace Curve
{
struct SCORE_PLUGIN_CURVE_EXPORT LinearSegmentData
{
};

class SCORE_PLUGIN_CURVE_EXPORT LinearSegment final : public SegmentModel
{
  MODEL_METADATA_IMPL(LinearSegment)
public:
  using data_type = LinearSegmentData;
  using SegmentModel::SegmentModel;

  LinearSegment(
      const LinearSegment& other, const id_type& id, QObject* parent);

  LinearSegment(DataStream::Deserializer& vis, QObject* parent)
      : SegmentModel{vis, parent}
  { vis.writeTo(*this); }

  LinearSegment(JSONObject::Deserializer& vis, QObject* parent)
      : SegmentModel{vis, parent}
  { vis.writeTo(*this); }

  void on_startChanged() override;
  void on_endChanged() override;

  void updateData(int numInterp) const override;
  double valueAt(double x) const override;

  QVariant toSegmentSpecificData() const override;

  ossia::curve_segment<float> makeFloatFunction() const override;
  ossia::curve_segment<int> makeIntFunction() const override;
  ossia::curve_segment<bool> makeBoolFunction() const override;
};
}

Q_DECLARE_METATYPE(Curve::LinearSegmentData)
