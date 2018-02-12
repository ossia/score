#pragma once
#include <QGraphicsItem>
#include <QPainter>
namespace Scenario
{
class IntervalModel;
class TimeBar : public QGraphicsItem
{
public:
  TimeBar(QGraphicsItem* parent);

  QRectF boundingRect() const override;
  IntervalModel* interval() const { return m_interval; }
  void setInterval(IntervalModel* itv) { m_interval = itv; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  bool playing = false;
private:
  IntervalModel* m_interval{};
};

}
