#pragma once
#include <QColor>
#include <QDateTime>
#include <QQuickPaintedItem>
#include <QMap>
#include <QPainterPath>
#include <QPoint>
#include <QStaticText>
#include <QString>
#include <QtGlobal>
#include <iscore/model/ColorReference.hpp>

class QGraphicsSceneMouseEvent;
class QPainter;

class QWidget;
class QGraphicsView;

namespace Scenario
{
class AbstractTimeRuler;
class AbstractTimeRulerView : public QQuickPaintedItem
{
  friend class AbstractTimeRuler;
  Q_OBJECT


  AbstractTimeRuler* m_pres{};

public:
  AbstractTimeRulerView(QWidget*);
  void setPresenter(AbstractTimeRuler* pres)
  {
    m_pres = pres;
  }
  void paint(
      QPainter* painter) override;

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

protected:
  void mousePressEvent(QMouseEvent*) final override;
  void mouseMoveEvent(QMouseEvent*) final override;
  void mouseReleaseEvent(QMouseEvent*) final override;
  void createRulerPath();

  struct Mark
  {
    double pos;
    QTime time;
    QStaticText text;
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
  QPointF m_lastScenePos;

  QPainterPath m_path;

  QWidget* m_viewport{};
};
}
