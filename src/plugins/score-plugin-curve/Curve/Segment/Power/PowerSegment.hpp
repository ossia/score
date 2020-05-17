#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/std/Optional.hpp>

#include <QVariant>

namespace Curve
{
class PowerSegment;
}

CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_CURVE_EXPORT,
    Curve::PowerSegment,
    "1e7cb83f-4e47-4b14-814d-2242a9c75991",
    "Power",
    "Power",
    "")

namespace Curve
{
struct SegmentData;
struct SCORE_PLUGIN_CURVE_EXPORT PowerSegmentData
{
  PowerSegmentData() = default;
  PowerSegmentData(double d) : gamma{d} { }

  // Value of gamma for which the pow will be == 1.
  static const constexpr double linearGamma = 1;
  double gamma = linearGamma;
};

class SCORE_PLUGIN_CURVE_EXPORT PowerSegment final : public SegmentModel
{
  MODEL_METADATA_IMPL(PowerSegment)
public:
  using data_type = PowerSegmentData;
  using SegmentModel::SegmentModel;
  PowerSegment(const SegmentData& dat, QObject* parent);

  PowerSegment(const PowerSegment& other, const id_type& id, QObject* parent);

  PowerSegment(DataStream::Deserializer& vis, QObject* parent) : SegmentModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  PowerSegment(JSONObject::Deserializer& vis, QObject* parent) : SegmentModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  double gamma = PowerSegmentData::linearGamma; // TODO private
private:
  void on_startChanged() override;
  void on_endChanged() override;

  void updateData(int numInterp) const override;
  double valueAt(double x) const override;

  optional<double> verticalParameter() const override;
  void setVerticalParameter(double p) override;

  QVariant toSegmentSpecificData() const override;

  template <typename Y>
  ossia::curve_segment<Y> makeFunction() const;

  ossia::curve_segment<double> makeDoubleFunction() const override;
  ossia::curve_segment<float> makeFloatFunction() const override;
  ossia::curve_segment<int> makeIntFunction() const override;
};
}

Q_DECLARE_METATYPE(Curve::PowerSegmentData)
W_REGISTER_ARGTYPE(Curve::PowerSegmentData)
