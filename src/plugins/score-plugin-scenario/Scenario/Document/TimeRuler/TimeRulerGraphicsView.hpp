#pragma once
#include <QGraphicsView>
namespace Scenario
{
class TimeRulerGraphicsView : public QGraphicsView
{
public:
  TimeRulerGraphicsView(QGraphicsScene*);

  void scrollContentsBy(int dx, int dy) override;
  void wheelEvent(QWheelEvent* event) override;
};
class MinimapGraphicsView final : public TimeRulerGraphicsView
{
public:
  MinimapGraphicsView(QGraphicsScene* s);

  void scrollContentsBy(int dx, int dy) override;
  void wheelEvent(QWheelEvent* event) override;
};
}
