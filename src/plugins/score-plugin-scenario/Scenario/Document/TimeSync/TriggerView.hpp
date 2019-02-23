#pragma once
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <score/widgets/MimeData.hpp>

#include <QGraphicsItem>
#include <QGraphicsSvgItem>
#include <QMimeData>
#include <QRect>

#include <score_plugin_scenario_export.h>
#include <wobjectdefs.h>
class QGraphicsSceneMouseEvent;
class QGraphicsSvgItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT TriggerView final : public QGraphicsSvgItem
{
  W_OBJECT(TriggerView)
  Q_INTERFACES(QGraphicsItem)

public:
  TriggerView(QGraphicsItem* parent);

  static constexpr int static_type()
  {
    return QGraphicsItem::UserType + ItemType::Trigger;
  }
  int type() const override { return static_type(); }

public:
  void pressed(QPointF arg_1) W_SIGNAL(pressed, arg_1);

  void dropReceived(const QPointF& pos, const QMimeData& arg_2)
      W_SIGNAL(dropReceived, pos, arg_2);

protected:
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
};
}
