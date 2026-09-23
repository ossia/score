#pragma once
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QByteArray>
#include <QPoint>
#include <QVector>

#include <score_plugin_curve_export.h>

#include <span>
#include <vector>

namespace score
{
class CommandStackFacade;
} // namespace score

/*
concept CommandObject
{
    public:
        void instantiate();
        void update();
        void commit();
        void rollback();
};
*/
// CreateSegment
// CreateSegmentBetweenPoints

// RemoveSegment -> easy peasy
// RemovePoint -> which segment do we merge ? At the left or at the right ?
// A point(view) has pointers to one or both of its curve segments.

namespace Curve
{
class UpdateCurve;
class Model;
class Presenter;
class StateBase;
class SegmentModel;

//! Debug builds: asserts that segts is a valid curve.
void checkValidity(std::span<const SegmentData> segts);

//! Adds a point at pt: splits the segment under it, moves the point already
//! at its x, or links it to the nearest segments around it.
SCORE_PLUGIN_CURVE_EXPORT
void createPointAt(std::vector<SegmentData>& segments, Curve::Point pt);

//! The curve without the given segments. With `fill` it is closed again: from
//! the start of a removed first segment, to the end of a removed last one, and
//! across each hole.
SCORE_PLUGIN_CURVE_EXPORT
std::vector<SegmentData> removeSegments(
    const Model& model, const ossia::hash_set<int32_t>& removed, bool fill);

class SCORE_PLUGIN_CURVE_EXPORT CommandObjectBase
{
public:
  CommandObjectBase(
      const Model& model, Presenter* pres, const score::CommandStackFacade&);
  virtual ~CommandObjectBase();

  void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }
  void press();

  void handleLocking();

protected:
  // Creates and pushes an UpdateCurve command
  // from a vector of segments.
  // They are removed afterwards
  void submit(const std::vector<SegmentData>&);

  auto find(std::vector<SegmentData>& segments, const OptionalId<SegmentModel>& id)
  {
    return std::find_if(
        segments.begin(), segments.end(), [&](const auto& seg) { return seg.id == id; });
  }
  auto find(const std::vector<SegmentData>& segments, const OptionalId<SegmentModel>& id)
  {
    return std::find_if(segments.cbegin(), segments.cend(), [&](const auto& seg) {
      return seg.id == id;
    });
  }

  virtual void on_press() = 0;

  QPointF m_originalPress; // Note : there should be only one per curve...

  const Model& m_model;
  Presenter* m_presenter{};

  Curve::StateBase* m_state{};

  SingleOngoingCommandDispatcher<UpdateCurve> m_dispatcher;

  std::vector<SegmentData> m_startSegments;
  //! The curve being submitted: kept across moves for its storage.
  std::vector<SegmentData> m_segments;

  // To prevent behind locked at 0.000001 or 0.9999
  double m_xmin{-1}, m_xmax{2}, m_xLastPoint{2};
};
}
