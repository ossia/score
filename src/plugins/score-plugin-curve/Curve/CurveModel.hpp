#pragma once
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <score/model/IdentifiedObject.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <score_plugin_curve_export.h>

#include <span>
#include <vector>
#include <verdigris>

class Selection;
namespace ossia
{
struct domain;
class value;
}
namespace Curve
{
class PointModel;
struct SegmentData;
class SCORE_PLUGIN_CURVE_EXPORT Model final : public IdentifiedObject<Model>
{
  SCORE_SERIALIZE_FRIENDS
  W_OBJECT(Model)
public:
  Model(const Id<Model>&, QObject* parent);
  ~Model();

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : IdentifiedObject{vis, parent}
  {
    vis.writeTo(*this);
  }

  //! Inserts a segment and recomputes the points: O(N), for building a curve
  //! a segment at a time. Edits go through applyChanges / fromCurveData.
  void addSegment(SegmentModel* m);
  void insertSegment(SegmentModel*);
  void removeSegment(SegmentModel* m);

  //! Takes ownership of freshly deserialized segments.
  void loadSegments(const std::vector<SegmentModel*>& models);

  //! Segments chain by chain, which for a valid curve is by increasing x.
  const std::vector<SegmentModel*>& sortedSegments() const noexcept;

  std::vector<SegmentData> toCurveData() const;
  //! Same, reusing the storage of `out`.
  void toCurveData(std::vector<SegmentData>& out) const;

  //! Replaces the curve. Refused, with a warning, if `curve` is not valid
  //! (see isValidCurve) or uses an unknown segment type.
  void fromCurveData(const std::vector<SegmentData>& curve);

  //! Removes the `removed` segments, then updates each of `upserted` in place
  //! (or creates it). Segments whose links do not change keep their points:
  //! the cost is that of the change, plus O(N) when the chains change.
  void applyChanges(
      std::span<const Id<SegmentModel>> removed,
      std::span<const SegmentData* const> upserted);

  Selection selectedChildren() const;
  void setSelection(const Selection& s);

  void clear();

  const auto& segments() const { return m_segments; }
  auto& segments() { return m_segments; }

  //! Chain by chain, which for a valid curve is by increasing x.
  const std::vector<PointModel*>& points() const;
  std::vector<PointModel*>& points() { return m_points; }

  double lastPointPos() const;

  std::optional<double> valueAt(double x) const noexcept;

public:
  void segmentAdded(const SegmentModel* arg_1)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, segmentAdded, arg_1)
  void segmentRemoved(const Id<SegmentModel>& arg_1) E_SIGNAL(
      SCORE_PLUGIN_CURVE_EXPORT, segmentRemoved,
      arg_1) // dangerous if async

  // This signal has to be emitted after big modifications.
  // (it's an optimization to prevent updating the OSSIA API each time a
  // segment moves).
  void changed() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, changed)
  void curveReset() E_SIGNAL(
      SCORE_PLUGIN_CURVE_EXPORT,
      curveReset) // like changed() but for the presenter
  void cleared() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, cleared)

private:
  //! Recomputes the segment order and the points from the segments' links.
  void relink();
  //! Same, after applyChanges: the order is spliced from the previous one with
  //! `added`, the segments it created, rather than rebuilt.
  void relinkAfterChanges(std::span<SegmentModel* const> added);
  bool spliceOrder(std::span<SegmentModel* const> added);
  bool rebuildOrder();
  void relinkPoints();
  void unlinkPoints(SegmentModel& m) noexcept;

  IdContainer<SegmentModel> m_segments;
  std::vector<PointModel*> m_points;
  std::vector<SegmentModel*> m_sorted;
  int32_t m_nextPointId{};
  uint32_t m_relinkPass{};
};

SCORE_PLUGIN_CURVE_EXPORT
std::vector<SegmentData> orderedSegments(const Model& curve);

//! Finite coordinates, segments that go forward in x, and previous / following
//! links that form chains. Anything else is refused by Model::fromCurveData.
SCORE_PLUGIN_CURVE_EXPORT
bool isValidCurve(std::span<const SegmentData> curve) noexcept;

struct SCORE_PLUGIN_CURVE_EXPORT CurveDomain
{
  CurveDomain() = default;
  CurveDomain(const CurveDomain&) = default;
  CurveDomain(CurveDomain&&) = default;
  CurveDomain& operator=(const CurveDomain&) = default;
  CurveDomain& operator=(CurveDomain&&) = default;
  CurveDomain(const ossia::domain& dom);
  CurveDomain(const ossia::domain& dom, const ossia::value&);
  CurveDomain(const ossia::domain& dom, double start, double end);
  CurveDomain(double start, double end)
      : min{std::min(start, end)}
      , max{std::max(start, end)}
      , start{start}
      , end{end}
  {
  }

  void refine(const ossia::domain&);
  void ensureValid();

  double min = 0;
  double max = 1;
  double start = 0;
  double end = 1;
};
}
