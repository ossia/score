#pragma once
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <score/widgets/MimeData.hpp>

#include <QGraphicsItem>
#include <QMimeData>

#include <score_plugin_scenario_export.h>

#include <verdigris>
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT TriggerView final : public QObject, public QGraphicsItem
{
  W_OBJECT(TriggerView)
  Q_INTERFACES(QGraphicsItem)

public:
  TriggerView(QGraphicsItem* parent);
  static const constexpr int Type = ItemType::Trigger;
  int type() const final override { return Type; }

  void setSelected(bool b) noexcept;
  void onWaitStart();
  void onWaitEnd();
  void nextFrame();

public:
  void pressed(QPointF arg_1) W_SIGNAL(pressed, arg_1);

  void dropReceived(const QPointF& pos, const QMimeData& arg_2) W_SIGNAL(dropReceived, pos, arg_2);

  QRectF boundingRect() const override;

private:
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
  bool contains(const QPointF& point) const override;

  void dropEvent(QGraphicsSceneDragDropEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;

  void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;

  const QPixmap& currentPixmap() const noexcept;

  int m_currentFrame{};
  int m_frameDirection{};

  bool m_selected : 1;
  bool m_hovered : 1;
  bool m_waiting : 1;
};
}
