#pragma once
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <score/widgets/MimeData.hpp>

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
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
    , public QGraphicsPixmapItem
{
  W_OBJECT(TriggerView)
  Q_INTERFACES(QGraphicsItem)

public:
  TriggerView(QGraphicsItem* parent);

  static constexpr int static_type()
  {
    return ItemType::Trigger;
  }
  int type() const override { return static_type(); }

  void setSelected(bool b) noexcept;

public:
  void pressed(QPointF arg_1) W_SIGNAL(pressed, arg_1);

  void dropReceived(const QPointF& pos, const QMimeData& arg_2)
      W_SIGNAL(dropReceived, pos, arg_2);

protected:
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;

  void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;

  bool m_selected: 1;
  bool m_hovered: 1;
};
}
