#pragma once

#include <QGraphicsItem>
#include <Process/TimeValue.hpp>

namespace Scenario
{
class Minimap
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
  public:
    Minimap(QWidget* vp);
    void setVisibleDuration(TimeVal t);
    void setWidth(double);
    void setLeftHandleTime(TimeVal t);
    void setRightHandleTime(TimeVal t);

  signals:
    void visibleRectChanged(QRectF);

  private:
    QRectF boundingRect() const;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

    void mousePressEvent(QGraphicsSceneMouseEvent*);
    void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*);

    QWidget* m_viewport{};
    TimeVal m_time;
    double m_leftHandle{};
    double m_rightHandle{};
    double m_height{100};
    double m_width{100};

    bool m_gripLeft{false};
    bool m_gripRight{false};
};
}
