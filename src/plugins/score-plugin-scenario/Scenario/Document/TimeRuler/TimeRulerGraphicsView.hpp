#pragma once
#include <QGraphicsView>
namespace Scenario
{
class TimeRulerGraphicsView : public QGraphicsView
{
public:
  explicit TimeRulerGraphicsView(QGraphicsScene*);

  void scrollContentsBy(int dx, int dy) override;
  void wheelEvent(QWheelEvent* event) override;
};
class MinimapGraphicsView final : public TimeRulerGraphicsView
{
public:
  explicit MinimapGraphicsView(QGraphicsScene* s);

  void scrollContentsBy(int dx, int dy) override;
  void wheelEvent(QWheelEvent* event) override;
};
}
