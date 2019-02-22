#pragma once
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <score/model/ColorReference.hpp>

#include <QGraphicsItem>
#include <QPainterPath>
#include <QRect>

#include <score_plugin_scenario_export.h>
#include <wobjectdefs.h>

namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT ConditionView final : public QObject,
                                                         public QGraphicsItem
{
  W_OBJECT(ConditionView)
  Q_INTERFACES(QGraphicsItem)
public:
  ConditionView(score::ColorRef color, QGraphicsItem* parent);

  using QGraphicsItem::QGraphicsItem;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter, const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  void changeHeight(qreal newH);

  void setColor(score::ColorRef c)
  {
    m_color = c;
    update();
  }

  static constexpr int static_type()
  {
    return QGraphicsItem::UserType + ItemType::Condition;
  }
  int type() const override
  {
    return static_type();
  }

public:
  void pressed(QPointF arg_1) W_SIGNAL(pressed, arg_1);

private:
  QPainterPath shape() const override;
  bool contains(const QPointF& point) const override;
  QPainterPath opaqueArea() const override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;

  score::ColorRef m_color;
  QPainterPath m_Cpath;
  QPainterPath m_strokedCpath;
  qreal m_height{0};
  qreal m_width{40};
  qreal m_CHeight{27};

};
}
