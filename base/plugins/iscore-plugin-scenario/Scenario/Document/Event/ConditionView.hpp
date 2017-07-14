#pragma once

#include <QGraphicsItem>
#include <QPainterPath>
#include <QRect>
#include <iscore/model/ColorReference.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>


class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{

class ConditionView final
        : public QObject
        , public QGraphicsItem
{
public:
  ConditionView(iscore::ColorRef color, QGraphicsItem* parent);

  using QGraphicsItem::QGraphicsItem;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  void changeHeight(qreal newH);

  void setColor(iscore::ColorRef c)
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

signals:
  void pressed();

private:
  iscore::ColorRef m_color;
  QPainterPath m_trianglePath;
  QPainterPath m_Cpath;
  qreal m_height{0};
  qreal m_width{40};
  qreal m_CHeight{27};
};
}
