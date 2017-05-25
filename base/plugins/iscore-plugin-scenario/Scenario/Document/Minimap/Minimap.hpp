#pragma once

#include <QGraphicsItem>
#include <Process/TimeValue.hpp>

namespace Scenario
{
class Minimap final
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
  public:
    Minimap(QWidget* vp);
    void setWidth(double);
    double width() const { return m_width; }
    double leftHandle() const { return m_leftHandle; }
    double rightHandle() const { return m_rightHandle; }

    // These do not send notification
    void setLeftHandle(double);
    void setRightHandle(double);
    void setHandles(double l, double r);

    // This one sends visibleRectChanged
    void modifyHandles(double l, double r);

    void setLargeView();
    void zoomIn();
    void zoomOut();
    void zoom(double z);

  signals:
    void visibleRectChanged(double l, double r);

  private:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)  override;

    void mousePressEvent(QGraphicsSceneMouseEvent*) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;

    QWidget* m_viewport{};
    double m_leftHandle{};
    double m_rightHandle{};
    static const constexpr double m_height{40};
    double m_width{100};
    QPoint m_startPos;
    QPointF m_lastPos;

    bool m_gripLeft{false};
    bool m_gripRight{false};
    bool m_gripMid{false};
};
}
