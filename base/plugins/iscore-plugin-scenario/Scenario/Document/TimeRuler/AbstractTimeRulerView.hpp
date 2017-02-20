#pragma once
#include <QColor>
#include <QDateTime>
#include <QQuickPaintedItem>
#include <QMap>
#include <QPainterPath>
#include <QPoint>
#include <QString>
#include <QtGlobal>
#include <iscore/model/ColorReference.hpp>

class QGraphicsSceneMouseEvent;
class QPainter;

class QWidget;

namespace Scenario
{
class AbstractTimeRuler;
class AbstractTimeRulerView : public QQuickPaintedItem
{
  Q_OBJECT


  AbstractTimeRuler* m_pres{};

public:
  AbstractTimeRulerView();
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

  qreal m_height{};
  qreal m_width{};

  qreal m_graduationsSpacing{};
  int m_graduationDelta{};
  QString m_timeFormat{};
  uint32_t m_intervalsBetweenMark{};

  qreal m_textPosition{};
  int m_graduationHeight{};

  iscore::ColorRef m_color;
  QPainterPath m_path;

  struct Mark
  {
    double pos;
    QTime time;
    QString text;
  };

  std::vector<Mark> m_marks;

  QPointF m_lastScenePos;
};
}
