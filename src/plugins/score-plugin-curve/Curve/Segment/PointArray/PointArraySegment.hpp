#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/serialization/VisitorCommon.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <ossia/detail/flat_map.hpp>

#include <QPoint>
#include <QVariant>
#include <QVector>

#include <memory>
#include <utility>
#include <vector>
#include <verdigris>

class QObject;
#include <score/model/Identifier.hpp>

namespace Curve
{
class PointArraySegment;
}

CURVE_SEGMENT_METADATA(
    SCORE_PLUGIN_CURVE_EXPORT,
    Curve::PointArraySegment,
    "c598b840-db67-4c8f-937a-46cfac87cb59",
    "PointArray",
    "PointArray",
    "hidden")

namespace Curve
{
class LinearSegment;
struct SegmentData;
struct PointArraySegmentData
{
  double min_x, max_x;
  double min_y, max_y;
  QVector<QPointF> m_points;
};
class SCORE_PLUGIN_CURVE_EXPORT PointArraySegment final : public SegmentModel
{
  W_OBJECT(PointArraySegment)
  MODEL_METADATA_IMPL(PointArraySegment)
public:
  using data_type = PointArraySegmentData;
  PointArraySegment(const Id<SegmentModel>& id, QObject* parent) : SegmentModel{id, parent} { }
  PointArraySegment(const SegmentData& dat, QObject* parent);

  PointArraySegment(const PointArraySegment& other, const id_type& id, QObject* parent);

  PointArraySegment(DataStream::Deserializer& vis, QObject* parent) : SegmentModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  PointArraySegment(JSONObject::Deserializer& vis, QObject* parent) : SegmentModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~PointArraySegment() override;

  void on_startChanged() override;
  void on_endChanged() override;

  void updateData(int numInterp) const override;
  double valueAt(double x) const override;

  void addPoint(double, double);
  void addPointUnscaled(double, double);
  void simplify(double ratio); // 10 is a good ratio
  std::vector<SegmentData> toLinearSegments() const;
  std::vector<SegmentData> toPowerSegments() const;

  double min() { return min_y; }
  double max() { return max_y; }

  void setMinX(double y) { min_x = y; }
  void setMinY(double y) { min_y = y; }
  void setMaxX(double y) { max_x = y; }
  void setMaxY(double y) { max_y = y; }

  const auto& points() const { return m_points; }

  QVariant toSegmentSpecificData() const override
  {
    PointArraySegmentData dat{min_x, max_x, min_y, max_y, {}};

    dat.m_points.reserve(m_points.size());
    for (const auto& pt : m_points)
      dat.m_points.push_back({pt.first, pt.second});

    return QVariant::fromValue(std::move(dat));
  }

  // This will throw if execution is attempted.
  ossia::curve_segment<double> makeDoubleFunction() const override { return {}; }
  ossia::curve_segment<float> makeFloatFunction() const override { return {}; }
  ossia::curve_segment<int> makeIntFunction() const override { return {}; }
  void reset();

public:
  void minChanged(double arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, minChanged, arg_1)
  void maxChanged(double arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, maxChanged, arg_1)

private:
  // Coordinates in {x, y}.
  double min_x{}, max_x{};
  double min_y{}, max_y{};

  double m_lastX{-1};

  ossia::flat_map<double, double> m_points;
};
}

Q_DECLARE_METATYPE(Curve::PointArraySegmentData)
W_REGISTER_ARGTYPE(Curve::PointArraySegmentData)
