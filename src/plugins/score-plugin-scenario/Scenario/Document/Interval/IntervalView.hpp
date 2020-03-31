#pragma once
#include <Scenario/Document/Interval/ExecutionState.hpp>
#include <Scenario/Document/Interval/Temporal/Braces/LeftBrace.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <score/model/ColorInterpolator.hpp>

#include <score/graphics/TextItem.hpp>

#include <QGraphicsItem>
#include <qnamespace.h>

#include <score_plugin_scenario_export.h>
#include <verdigris>
class QGraphicsSceneMouseEvent;

namespace Process
{
struct Style;
}
namespace score
{
class SimpleTextItem;
}
namespace Scenario
{
class IntervalPresenter;
class LeftBraceView;
class RightBraceView;
class IntervalMenuOverlay;
class SCORE_PLUGIN_SCENARIO_EXPORT IntervalView : public QObject,
                                                  public QGraphicsItem
{
  W_OBJECT(IntervalView)
  Q_INTERFACES(QGraphicsItem)

public:
  IntervalView(IntervalPresenter& presenter, QGraphicsItem* parent);
  virtual ~IntervalView();

  static constexpr int static_type()
  {
    return ItemType::Interval;
  }
  int type() const final override { return static_type(); }

  const IntervalPresenter& presenter() const { return m_presenter; }

  void setInfinite(bool);
  bool infinite() const { return m_infinite; }

  void setExecuting(bool);
  void setDefaultWidth(double width);
  void setMaxWidth(bool infinite, double max);
  void setMinWidth(double min);
  void setHeight(double height);
  double setPlayWidth(double width);
  void setValid(bool val);

  double height() const { return m_height; }

  bool isSelected() const { return m_selected; }

  double defaultWidth() const { return m_defaultWidth; }

  double minWidth() const { return m_minWidth; }

  double maxWidth() const { return m_maxWidth; }

  double intervalAndRackHeight() const { return m_height; }

  double playWidth() const { return m_playWidth; }

  bool isValid() const { return m_validInterval; }

  bool warning() const;
  void setWarning(bool warning);

  void setExecutionState(IntervalExecutionState);
  const score::Brush& intervalColor(const Process::Style& skin) const;

  void updateLabelPos();
  void updateCounterPos();
  virtual void updatePaths() = 0;
  virtual void updatePlayPaths() = 0;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

  LeftBraceView& leftBrace() { return m_leftBrace; }
  RightBraceView& rightBrace() { return m_rightBrace; }

  void setDropTarget(bool b)
  {
     m_dropTarget = b;
     update();
  }

public:
  void requestOverlayMenu(QPointF arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, requestOverlayMenu, arg_1)
  void dropReceived(const QPointF& pos, const QMimeData& arg_2)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, dropReceived, pos, arg_2)

protected:
  friend class TemporalIntervalHeader;
  friend class FullViewIntervalHeader;
  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  QPainterPath shape() const override;
  QPainterPath opaqueArea() const override;
  bool contains(const QPointF&) const override;

  void setGripCursor();
  void setUngripCursor();

  LeftBraceView m_leftBrace;
  RightBraceView m_rightBrace;
  score::SimpleTextItem m_counterItem;

  IntervalPresenter& m_presenter;
  QPainterPath solidPath, playedSolidPath;

  double m_defaultWidth{};
  double m_maxWidth{};
  double m_minWidth{};
  double m_playWidth{};
  double m_height{};
  score::ColorBang m_execPing;

  bool m_selected : 1;
  bool m_infinite : 1;
  bool m_validInterval : 1;//{true};
  bool m_warning : 1;
  bool m_waiting : 1;
  bool m_dropTarget : 1;
  IntervalExecutionState m_state : 2;

};
}
