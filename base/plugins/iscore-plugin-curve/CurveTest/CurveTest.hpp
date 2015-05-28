#include <QGraphicsItem>
//namespace {
    class MyPoint;
    class PointsLayer;
//}
class Curve : public QGraphicsItem
{
    QRectF rect; // The rect in which the whole curve must fit.
    QVector<QPointF> m_points; // Each between 0, 1

    friend class ::MyPoint;
    public:
    using QGraphicsItem::QGraphicsItem;
        void setRect(const QRectF& theRect)
        {
            prepareGeometryChange();
            rect = theRect;
            updateSubitems();
        }

        void setPoints(const QVector<QPointF>& thePoints)
        {
            m_points = thePoints;
            updateSubitems();
        }

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

        void updateSubitems();

        /*
        void setCurrentPointPos(const QPointF &point);
        void removeFakePoint();
        void pointMoved(const QPointF& pt);
        QPointF pointUnderMouse(QGraphicsSceneMouseEvent *event);
    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event);
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event);


        QSizeF m_size;
        QPointF m_backedUpPoint{-1, -1};

        //QPen m_pen;
        PointsLayer* m_pointLayer{};
        MyPoint* m_fakePoint{};
        */
};

class CurveSegmentTest : public QGraphicsItem
{
        // Takes a table of points and draws them in a square given by the boundingRect
        // QGraphicsItem interface
        QVector<QPointF> points; // each between rect.topLeft() :: rect.bottomRight()
        QRectF rect;
    public:
        using QGraphicsItem::QGraphicsItem;

        void setRect(const QRectF& theRect)
        {
            prepareGeometryChange();
            rect = theRect;
        }

        void setPoints(const QVector<QPointF>& thePoints)
        {
            points = thePoints;
        }

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};
