#pragma once
#include <Process/TimeValue.hpp>
#include <Scenario/Document/TimeRuler/TimeRuler.hpp>

#include <score/model/ColorReference.hpp>

#include <ossia/editor/scenario/time_value.hpp>

#include <QGlyphRun>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QPoint>
#include <QTextLayout>

#include <score_plugin_scenario_export.h>
#include <verdigris>

#include <chrono>
#include <deque>
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class QGraphicsView;

namespace Scenario
{
class MusicalGrid;
class TimeRulerBase
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(TimeRulerBase)
public:
  qreal width() const { return m_width; }
  void setWidth(qreal newWidth);
  void setStartPoint(ossia::time_value dur);
  virtual void setZoomRatio(double factor) = 0;
  virtual void setGrid(MusicalGrid& grid);

  void drag(QPointF arg_1, QPointF arg_2) W_SIGNAL(drag, arg_1, arg_2);
  void rescale() W_SIGNAL(rescale);



protected:
  virtual void createRulerPath() = 0;
  virtual void computeGraduationSpacing() = 0;
  QRectF boundingRect() const final override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) final override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent*) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent*) final override;
  ossia::time_value m_startPoint{};
  qreal m_width{};
};

class TimeRuler final
    : public TimeRulerBase
{
  Q_INTERFACES(QGraphicsItem)

public:
  enum class Format
  {
    Hours,
    Seconds,
    Milliseconds,
    Microseconds,
  };

  TimeRuler(QGraphicsView*);

private:
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  qreal graduationSpacing() const
  {
    return m_intervalsBetweenMark * m_graduationsSpacing;
  }

  void computeGraduationSpacing() override;
  void createRulerPath() override;
  void setZoomRatio(double factor) final override;

  struct Mark
  {
    double pos;
    std::chrono::nanoseconds time;
    QGlyphRun text;
  };

  double m_pixelPerMillis{0.01};

  std::vector<Mark> m_marks;


  qreal m_graduationsSpacing{};
  qreal m_graduationDelta{};
  qreal m_intervalsBetweenMark{};
  Format m_timeFormat{};

  QPainterPath m_path;

  QGraphicsView* m_viewport{};

  QGlyphRun getGlyphs(std::chrono::nanoseconds);
  QTextLayout m_layout;
  std::deque<std::pair<std::chrono::nanoseconds, QGlyphRun>> m_stringCache;
};


class MusicalRuler final
    : public TimeRulerBase
{
  Q_INTERFACES(QGraphicsItem)

public:
  enum class Format
  {
    Bars,
    Quarters,
    Semiquavers,
    Cents,
  };

  MusicalRuler(QGraphicsView*);

private:
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;


  void computeGraduationSpacing() override;
  void createRulerPath() override;
  void setZoomRatio(double factor) final override;
  void setGrid(MusicalGrid& grid) final override;

  MusicalGrid* m_grid{};

  QGraphicsView* m_viewport{};

  QGlyphRun getGlyphs(ossia::bar_time timings, ossia::bar_time increments);
  QTextLayout m_layout;
  std::deque<std::tuple<ossia::bar_time, ossia::bar_time, QGlyphRun>> m_stringCache;
};
}
