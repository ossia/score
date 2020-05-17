#pragma once
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <QObject>
#include <QPoint>
#include <QRect>

#include <score_plugin_curve_export.h>

#include <verdigris>

class QActionGroup;
class QMenu;
namespace Curve
{
class SegmentList;
struct Style;
class View;
class Model;

class SCORE_PLUGIN_CURVE_EXPORT Presenter final : public QObject
{
  W_OBJECT(Presenter)
public:
  Presenter(
      const score::DocumentContext& lst,
      const Curve::Style&,
      const Model&,
      View*,
      QObject* parent);
  virtual ~Presenter();

  const score::DocumentContext& context() const noexcept
  {
    return m_commandDispatcher.stack().context();
  }

  const auto& points() const noexcept { return m_points; }
  const auto& segments() const noexcept { return m_segments; }

  // Removes all the points & segments
  void clear();

  const Model& model() const noexcept { return m_model; }
  View& view() const noexcept { return *m_view; }

  void setRect(const QRectF& rect);

  void enableActions(bool);

  // Changes the colors
  void enable();
  void disable();

  Curve::EditionSettings& editionSettings() noexcept { return m_editionSettings; }

  void fillContextMenu(QMenu&, const QPoint&, const QPointF&);

  void removeSelection();

  // Used to allow moving outside [0; 1] when in the panel view.
  bool boundedMove() const noexcept { return m_boundedMove; }
  void setBoundedMove(bool b) noexcept { m_boundedMove = b; }

  QRectF rect() const noexcept { return m_localRect; }

public:
  void contextMenuRequested(const QPoint& arg_1, const QPointF& arg_2)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, contextMenuRequested, arg_1, arg_2)

private:
  // Context menu actions
  void updateSegmentsType(const UuidKey<Curve::SegmentFactory>& segment);

  // Setup utilities
  void setPos(PointView&);
  void setPos(SegmentView&);
  void setupSignals();
  void setupView();
  void setupStateMachine();

  // Adding
  void addPoint(PointView*);
  void addSegment(SegmentView*);

  void addPoint_impl(PointView*);
  void addSegment_impl(SegmentView*);

  void setupPointConnections(PointView*);
  void setupSegmentConnections(SegmentView*);
  void modelReset();

  const SegmentList& m_curveSegments;
  QRectF m_localRect;

  const Model& m_model;
  graphics_item_ptr<View> m_view;

  IdContainer<PointView, PointModel> m_points;
  IdContainer<SegmentView, SegmentModel> m_segments;

  // Required dispatchers
  CommandDispatcher<> m_commandDispatcher;

  const Curve::Style& m_style;
  Curve::EditionSettings& m_editionSettings;

  QMenu* m_contextMenu{};

  bool m_enabled = true;
  bool m_boundedMove = true;
};
}
