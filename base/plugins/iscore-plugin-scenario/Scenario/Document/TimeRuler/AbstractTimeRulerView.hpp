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
#include <iscore/model/ColorReference.hpp>
#include <deque>
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class QGraphicsView;

namespace Scenario
{
class AbstractTimeRuler;
class AbstractTimeRulerView : public QObject, public QGraphicsItem
{
  friend class AbstractTimeRuler;
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)

  AbstractTimeRuler* m_pres{};

public:
  AbstractTimeRulerView(QGraphicsView*);
  void setPresenter(AbstractTimeRuler* pres)
  {
    m_pres = pres;
  }
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

  void setGraduationsStyle(double size, int delta, QString format, int mark);
  void setFormat(QString);

signals:
  void drag(QPointF, QPointF);
  void rescale();

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent*) final override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent*) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent*) final override;
  void createRulerPath();

  struct Mark
  {
    double pos;
    QTime time;
    QGlyphRun text;
  };

  std::vector<Mark> m_marks;

  qreal m_height{};
  qreal m_width{};

  qreal m_graduationsSpacing{};
  qreal m_textPosition{};
  int32_t m_graduationDelta{};
  int32_t m_graduationHeight{};
  uint32_t m_intervalsBetweenMark{};
  QString m_timeFormat{};

  QPainterPath m_path;

  QWidget* m_viewport{};

  QGlyphRun getGlyphs(const QTime& t);
  QTextLayout m_layout;
  std::deque<std::pair<QTime, QGlyphRun>> m_stringCache;
};
}
