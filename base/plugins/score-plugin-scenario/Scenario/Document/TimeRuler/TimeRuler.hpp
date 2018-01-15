#pragma once
#include <QColor>
#include <QDateTime>
#include <QGlyphRun>
#include <QGraphicsItem>
#include <QMap>
#include <QPainterPath>
#include <QPoint>
#include <QCache>
#include <QTextLayout>
#include <QString>
#include <QtGlobal>
#include <score/model/ColorReference.hpp>
#include <deque>
#include <chrono>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/TimeRuler/TimeRuler.hpp>
#include <ossia/editor/scenario/time_value.hpp>
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class QGraphicsView;

namespace Scenario
{
class TimeRuler final : public QObject, public QGraphicsItem
{
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

public:
  enum class Format { Seconds, Milliseconds };

  TimeRuler(QGraphicsView*);
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setHeight(qreal newHeight);
  void setWidth(qreal newWidth);

  qreal graduationSpacing() const
  {
    return m_intervalsBetweenMark * m_graduationsSpacing;
  }

  qreal width() const
  {
    return m_width;
  }


  void setStartPoint(TimeVal dur);
  void setPixelPerMillis(double factor);

Q_SIGNALS:
  void drag(QPointF, QPointF);
  void rescale();

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
    std::chrono::microseconds time;
    QGlyphRun text;
  };

  ossia::time_value m_startPoint{};

  double m_pixelPerMillis{0.01};

  std::vector<Mark> m_marks;

  qreal m_height{};
  qreal m_width{};

  qreal m_graduationsSpacing{};
  qreal m_textPosition{};
  qreal m_graduationDelta{};
  qreal m_graduationHeight{};
  qreal m_intervalsBetweenMark{};
  Format m_timeFormat{};

  QPainterPath m_path;

  QWidget* m_viewport{};

  QGlyphRun getGlyphs(std::chrono::microseconds);
  QTextLayout m_layout;
  std::deque<std::pair<std::chrono::microseconds, QGlyphRun>> m_stringCache;
};
}
