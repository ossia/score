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
class SCORE_PLUGIN_SCENARIO_EXPORT TriggerView final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(TriggerView)
  Q_INTERFACES(QGraphicsItem)

public:
  TriggerView(QGraphicsItem* parent);
  static const constexpr int Type = ItemType::Trigger;
  int type() const final override { return Type; }

  void setSelected(bool b) noexcept;

public:
  void pressed(QPointF arg_1) W_SIGNAL(pressed, arg_1);

  void dropReceived(const QPointF& pos, const QMimeData& arg_2)
      W_SIGNAL(dropReceived, pos, arg_2);

  QRectF boundingRect() const;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;
  void nextFrame();
private:
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;

  void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;

  void updatePixmap();

  void onWaitStart();
  void onWaitEnd();

  bool m_selected: 1;
  bool m_hovered: 1;
  bool m_waiting: 1;

  QPixmap m_currentPixmap;
  int m_currentFrame;
  int m_frameDirection;
};
}
