#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>

namespace Curve
{
class NoiseSegment;
}

CURVE_SEGMENT_METADATA(
    ISCORE_PLUGIN_CURVE_EXPORT,
    Curve::NoiseSegment,
    "82a3709d-d9cc-4dcf-9aa8-5d539545f34b",
    "Noise",
    "Noise", "")

namespace Curve
{
struct ISCORE_PLUGIN_CURVE_EXPORT NoiseSegmentData
{
};

class ISCORE_PLUGIN_CURVE_EXPORT NoiseSegment final : public SegmentModel
{
  MODEL_METADATA_IMPL(NoiseSegment)
public:
  using data_type = NoiseSegmentData;
  using SegmentModel::SegmentModel;

  NoiseSegment(
      const NoiseSegment& other, const id_type& id, QObject* parent);

  NoiseSegment(DataStream::Deserializer& vis, QObject* parent)
      : SegmentModel{vis, parent}
  { vis.writeTo(*this); }

  NoiseSegment(JSONObject::Deserializer& vis, QObject* parent)
      : SegmentModel{vis, parent}
  { vis.writeTo(*this); }

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

Q_DECLARE_METATYPE(Curve::NoiseSegmentData)
