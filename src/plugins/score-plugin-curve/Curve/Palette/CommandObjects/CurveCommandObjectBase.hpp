#pragma once
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>

#include <QByteArray>
#include <QPoint>
#include <QVector>

#include <score_plugin_curve_export.h>

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

class SCORE_PLUGIN_CURVE_EXPORT CommandObjectBase
{
public:
  CommandObjectBase(Presenter* pres, const score::CommandStackFacade&);
  virtual ~CommandObjectBase();

  void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }
  void press();

  void handleLocking();

  // Creates and pushes an UpdateCurve command
  // from a vector of segments.
  // They are removed afterwards
  void submit(std::vector<SegmentData>&&);

protected:
  auto find(std::vector<SegmentData>& segments, const OptionalId<SegmentModel>& id)
  {
    return std::find_if(
        segments.begin(), segments.end(), [&](const auto& seg) { return seg.id == id; });
  }
  auto find(const std::vector<SegmentData>& segments, const OptionalId<SegmentModel>& id)
  {
    return std::find_if(
        segments.cbegin(), segments.cend(), [&](const auto& seg) { return seg.id == id; });
  }

  virtual void on_press() = 0;

  QVector<QByteArray> m_oldCurveData;
  QPointF m_originalPress; // Note : there should be only one per curve...

  Presenter* m_presenter{};

  Curve::StateBase* m_state{};

  SingleOngoingCommandDispatcher<UpdateCurve> m_dispatcher;

  std::vector<SegmentData> m_startSegments;

  // To prevent behind locked at 0.000001 or 0.9999
  double m_xmin{-1}, m_xmax{2}, m_xLastPoint{2};
};
}
