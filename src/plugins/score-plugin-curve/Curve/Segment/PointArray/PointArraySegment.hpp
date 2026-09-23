#pragma once
#include <Curve/Envelope.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/serialization/VisitorCommon.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <ossia/detail/flat_map.hpp>

#include <QPoint>
#include <QVariant>
#include <QVector>

#include <memory>
#include <span>
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
    SCORE_PLUGIN_CURVE_EXPORT, Curve::PointArraySegment,
    "c598b840-db67-4c8f-937a-46cfac87cb59", "PointArray", "PointArray", "hidden")

namespace Curve
{
class LinearSegment;
struct SegmentData;
class SegmentIdAllocator;

//! Samples as x -> y, in the units of their source: a PointArraySegment maps
//! [min_x, max_x] to its own extent in the curve and [min_y, max_y] to [0, 1].
using PointArraySamples = ossia::flat_map<double, double>;

//! The samples are shared, never modified once shared: copying the data, the
//! segment or the curve, and handing it to the executor, is O(1).
struct SCORE_PLUGIN_CURVE_EXPORT PointArraySegmentData
{
  double min_x{}, max_x{};
  double min_y{}, max_y{};
  std::shared_ptr<const PointArraySamples> points;

  bool operator==(const PointArraySegmentData& other) const noexcept;
};

//! A curve given by samples rather than by points: one segment for data that
//! would be too many segments to edit, such as a large imported file.
//! Evaluated by interpolating linearly between the samples.
class SCORE_PLUGIN_CURVE_EXPORT PointArraySegment final : public SegmentModel
{
  W_OBJECT(PointArraySegment)
public:
  MODEL_METADATA_IMPL(PointArraySegment)

  using data_type = PointArraySegmentData;
  PointArraySegment(const Id<SegmentModel>& id, QObject* parent);
  PointArraySegment(const SegmentData& dat, QObject* parent);

  PointArraySegment(const PointArraySegment& other, const id_type& id, QObject* parent);

  PointArraySegment(DataStream::Deserializer& vis, QObject* parent);
  PointArraySegment(JSONObject::Deserializer& vis, QObject* parent);

  ~PointArraySegment() override;

  void on_startChanged() override;
  void on_endChanged() override;

  //! The envelope of the samples, numInterp columns across the segment.
  void updateData(int numInterp) const override;

  //! The envelope of the samples between curve abscissas x0 and x1, `columns`
  //! across, in curve coordinates: O(columns log N).
  void envelope(double x0, double x1, int columns, std::vector<QPointF>& out) const;

  //! The number of samples between curve abscissas x0 and x1.
  std::size_t samplesBetween(double x0, double x1) const noexcept;

  //! One vertical line per column, see Curve::envelopeColumns; x0 and width
  //! in curve coordinates, the lines in curve coordinates.
  void envelopeColumns(
      double x0, double width, int columns, std::vector<QLineF>& out) const;
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

  const PointArraySamples& points() const noexcept { return *m_points; }
  void setData(const PointArraySegmentData& dat);
  void setSpecificData(const QVariant& data) override;
  bool specificDataEquals(const QVariant& data) const noexcept override;
  void reserve(std::size_t p);

  QVariant toSegmentSpecificData() const override;

  ossia::curve_segment<double> makeDoubleFunction() const override;
  ossia::curve_segment<float> makeFloatFunction() const override;
  ossia::curve_segment<int> makeIntFunction() const override;
  ossia::curve_segment<double>
  makeScaledDoubleFunction(double offset, double factor) const override;
  ossia::curve_segment<float>
  makeScaledFloatFunction(double offset, double factor) const override;
  ossia::curve_segment<int>
  makeScaledIntFunction(double offset, double factor) const override;
  void reset();

public:
  void minChanged(double arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, minChanged, arg_1)
  void maxChanged(double arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, maxChanged, arg_1)

private:
  PointArraySamples& editPoints();
  //! Every sample within the segment, in curve coordinates.
  std::vector<QPointF> mappedPoints() const;

  // Coordinates in {x, y}.
  double min_x{}, max_x{};
  double min_y{}, max_y{};

  double m_lastX{-1};

  std::shared_ptr<const PointArraySamples> m_points;
  mutable int m_dataResolution{-1};
  //! The pyramid of the current samples, built on first use.
  const MinMaxPyramid& pyramid() const;
  mutable MinMaxPyramid m_pyramid;
  // The samples it was built from: a weak_ptr is never equal to a later
  // buffer, even one allocated at the same address. Reset on in-place edits.
  mutable std::weak_ptr<const PointArraySamples> m_pyramidOf;
};

//! Changes the extent of a segment. The samples of a point array keep their
//! place in the curve: it is cropped, not stretched.
SCORE_PLUGIN_CURVE_EXPORT
void setSegmentExtent(SegmentData& seg, QPointF start, QPointF end);

//! A curve through `values`, each in [0, 1], evenly spaced over [0, 1]: linear
//! segments that can be edited one by one, or above `editable_max` values a
//! single sampled segment. Non-finite values are skipped.
SCORE_PLUGIN_CURVE_EXPORT
std::vector<SegmentData>
curveFromSamples(std::span<const float> values, std::size_t editable_max = 10000);

//! Linear segments that follow a point array within `tolerance` in y, as a
//! chain with fresh ids from `ids`, in place of `pointArray`.
SCORE_PLUGIN_CURVE_EXPORT
std::vector<SegmentData> editableSegments(
    const SegmentData& pointArray, double tolerance, SegmentIdAllocator& ids);
}

SCORE_SERIALIZE_DATASTREAM_DECLARE(
    SCORE_PLUGIN_CURVE_EXPORT, Curve::PointArraySegmentData)
Q_DECLARE_METATYPE(Curve::PointArraySegmentData)
W_REGISTER_ARGTYPE(Curve::PointArraySegmentData)
