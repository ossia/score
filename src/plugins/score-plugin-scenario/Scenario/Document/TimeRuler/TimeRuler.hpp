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
class SCORE_PLUGIN_SCENARIO_EXPORT TimeRuler final : public QObject,
                                                     public QGraphicsItem
{
  W_OBJECT(TimeRuler)
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
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setWidth(qreal newWidth);

  qreal graduationSpacing() const
  {
    return m_intervalsBetweenMark * m_graduationsSpacing;
  }

  qreal width() const { return m_width; }

  void setStartPoint(std::chrono::nanoseconds dur);
  void setPixelPerMillis(double factor);

public:
  void drag(QPointF arg_1, QPointF arg_2) W_SIGNAL(drag, arg_1, arg_2);
  void rescale() W_SIGNAL(rescale);

private:
  void computeGraduationSpacing();

  QRectF boundingRect() const final override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) final override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent*) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent*) final override;
  void createRulerPath();

  struct Mark
  {
    double pos;
    std::chrono::nanoseconds time;
    QGlyphRun text;
  };

  std::chrono::nanoseconds m_startPoint{};

  double m_pixelPerMillis{0.01};

  std::vector<Mark> m_marks;

  qreal m_width{};

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
}
