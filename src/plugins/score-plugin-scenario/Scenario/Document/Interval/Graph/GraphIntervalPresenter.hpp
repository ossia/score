#pragma once
#include <QObject>
#include <QGraphicsItem>
#include <score/model/Identifier.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <verdigris>
#include <QPainterPath>
#include <score_plugin_scenario_export.h>

namespace Process
{
struct Context;
}
namespace Scenario
{
class IntervalModel;
class StateView;
class GraphalIntervalPresenter
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(GraphalIntervalPresenter)

public:
  GraphalIntervalPresenter(const IntervalModel& model,
                           const StateView& start,
                           const StateView& end,
                           const Process::Context& ctx,
                           QGraphicsItem* parent = nullptr);

  const Id<IntervalModel>& id() const;
  const IntervalModel& model() const;

  static constexpr int static_type()
  {
    return ItemType::GraphInterval;
  }
  int type() const final override { return static_type(); }


  void pressed(QPointF arg_1) const
  E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, pressed, arg_1)
  void moved(QPointF arg_1) const
  E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, moved, arg_1)
  void released(QPointF arg_1) const
  E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, released, arg_1)

  QRectF boundingRect() const override;

  void resize();

  void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;
  QPainterPath shape() const override;
  QPainterPath opaqueArea() const override;
  bool contains(const QPointF& point) const override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  const IntervalModel& m_model;
  const StateView& m_start;
  const StateView& m_end;
  const Process::Context& m_context;
  QPainterPath m_path;
};
}
