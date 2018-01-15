#pragma once

#include <QGraphicsItem>
#include <Process/TimeValue.hpp>
class QGraphicsView;
namespace Scenario
{
class Minimap final
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
  public:
    Minimap(QGraphicsView* vp);
    void setWidth(double);
    double width() const { return m_width; }
    double leftHandle() const { return m_leftHandle; }
    double rightHandle() const { return m_rightHandle; }

    // These do not send notification
    void setMinDistance(double);
    void setLeftHandle(double);
    void setRightHandle(double);
    void setHandles(double l, double r);

    // This one sends visibleRectChanged
    void modifyHandles(double l, double r);

    void setLargeView();
    void zoomIn();
    void zoomOut();
    void zoom(double z);

  Q_SIGNALS:
    void rescale();
    void visibleRectChanged(double l, double r);

  private:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)  override;

    void mousePressEvent(QGraphicsSceneMouseEvent*) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) final override;

    void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent*) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;

    static const constexpr double m_height{40.};

    QGraphicsView* m_viewport{};
    double m_leftHandle{};
    double m_rightHandle{};
    double m_width{100.};
    double m_minDist{10.};
    QPoint m_startPos;
    double m_startY{};
    double m_relativeStartX{};

    bool m_gripLeft{false};
    bool m_gripRight{false};
    bool m_gripMid{false};
    bool m_setCursor{false};
};
}
