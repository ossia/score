#include "CurveTest.hpp"
#include <QPainter>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>

inline double clamp(double val, double min, double max)
{
    return val < min ? min : (val > max ? max : val);
}

#include <QApplication>
//namespace {
static QColor outerColor{Qt::white};
static const QColor innerColor{255, 200, 200};
static QPen enabledPen{outerColor, 3};
static const QPen disabledPen{QColor{100, 100, 100, 200}};

class MyPoint : public QGraphicsItem
{
    public:
        QPointF originalPoint;
        QPointF previousPoint;

        MyPoint(QPointF coordPos, QPointF pixelPos, CurveView* parent):
            QGraphicsItem{parent},
            originalPoint{coordPos},
            previousPoint{coordPos},
            m_rect{parent->boundingRect()},
            m_pointPos{pixelPos},
            m_curve{*parent}
        {
            setZValue(3);

            setAcceptHoverEvents(true);
            setFlags(ItemIsMovable);
            setFlag(ItemSendsGeometryChanges);
        }

        QPointF clampPoint(const QPointF& point)
        {
            /*
            QPointF movedPointInCoord = { m_curve->m_plot->xAxis->pixelToCoord(point.x()),
                                          m_curve->m_plot->yAxis->pixelToCoord(point.y())};
            */
            QPointF movedPointInCoord = { point.x() / m_curve.rect.width(), point.y() / m_curve.rect.height()};

            auto newX = clamp(movedPointInCoord.x(), 0.0, 1.0);
            if(originalPoint.x() == 0. || originalPoint.x() == 1.)
            {
                newX = originalPoint.x();
            }
            auto newY = clamp(movedPointInCoord.y(), 0.0, 1.0);

            /*
            return QPointF{m_curve->m_plot->xAxis->coordToPixel(newX),
                        m_curve->m_plot->yAxis->coordToPixel(newY)};
                        */
            return { newX * m_curve.rect.width(), newY * m_curve.rect.height()};
        }

        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override
        {
            painter->setPen(Qt::blue);
            painter->setBrush(Qt::green);
            painter->drawEllipse(m_pointPos, 7, 7);

            painter->setPen(outerColor);
            painter->setBrush(innerColor);
            painter->drawEllipse(m_pointPos, 5, 5);
        }

        QRectF boundingRect() const override
        {
            return m_rect;
        }

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override
        {
            event->accept();
        }

        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
        {
            m_pointPos = clampPoint(event->pos());
            //m_curve->setCurrentPointPos(m_pointPos);
            update();
        }

        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
        {
            //m_curve->pointMoved(clampPoint(event->pos()));
        }

        void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override
        {
            using namespace std;
            double x = event->pos().x();
            double y = event->pos().y();

            if(!((x <= m_pointPos.x() + 7)
                 && (x >= m_pointPos.x() - 7)
                 && (y <= m_pointPos.y() + 7)
                 && (y >= m_pointPos.y() - 7)))
            {
                //m_curve->removeFakePoint();
            }
        }

    private:
        QRectF m_rect;
        QPointF m_pointPos;
        CurveView& m_curve;
};

class PointsLayer : public QGraphicsItem
{
        QRectF m_rect;
    public:
        void enable()
        {
            setVisible(true);
            update();
        }

        void disable()
        {
            setVisible(false);
            update();
        }

        bool enabled() const
        {
            return isVisible();
        }

        PointsLayer(CurveView* parent):
            QGraphicsItem{parent}
        {
            setRect(parent->boundingRect());
        }

        // Points in pixel coordinates
        void setPoints(const QVector<QPointF>& points)
        {
            m_points = points;
            update();
        }

        void setRect(const QRectF& rect)
        {
            prepareGeometryChange();
            m_rect = rect;
        }

        QRectF boundingRect() const override
        { return m_rect; }


        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override
        {
            painter->setPen(outerColor);
            painter->setBrush(outerColor);

            for(const auto& pt : m_points)
                painter->drawEllipse(pt, 3, 3);
        }

    private:
        QVector<QPointF> m_points;
};
//}

static const QPointF invalid_point{-1, -1};






QRectF CurveSegmentView::boundingRect() const
{
    return rect;
}

void CurveSegmentView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::red);
    for(int i = 0; i < points.size() - 1; i++)
        painter->drawLine(points[i], points[i+1]);

    painter->setPen(Qt::magenta);
    painter->setBrush(Qt::transparent);
    painter->drawRect(boundingRect());
}


QRectF CurveView::boundingRect() const
{
    return rect;
}


QPointF myscale(QPointF first, QSizeF second)
{
    return {first.x() * second.width(), first.y() * second.height()};
}

void CurveView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::magenta);
    painter->setBrush(Qt::transparent);
    painter->drawRect(boundingRect());
}
#include <QDebug>
void CurveView::updateSubitems()
{
    /*
    for(auto& item : this->childItems())
        delete item;

    // To compute with the rect and the size in pixels instead.
    // Also, for a line, needs only be 1.
    double numInterp = 10.;

    QVector<QPointF> scaledPoints;
    for(const auto& point : m_points)
    {
        scaledPoints.append(myscale(point, boundingRect().size()));
    }

    for(int i = 0; i < scaledPoints.size() - 1; i++)
    {
        auto lerpFun = [&] (double percent)
        {
            return scaledPoints[i] + percent * (scaledPoints[i+1] - scaledPoints[i]);
        };

        auto c = new CurveSegmentView(this);
        QVector<QPointF> interppts;
        interppts.resize(numInterp + 1);

        for(int j = 0; j < numInterp; j++)
        {
            interppts[j] = lerpFun(j / numInterp);
            if(j == numInterp - 1)
            {
                interppts[numInterp] = scaledPoints[i+1];
            }
        }

        c->setPoints(interppts);
        c->setRect(QRectF(interppts.first(), interppts.last()).normalized());
    }

    auto ptlayer = new PointsLayer(this);
    ptlayer->setPoints(scaledPoints);
    */
}

/*

void Curve::setCurrentPointPos(const QPointF& point)
{
    // TODO que se passe-t-il quand on déplace des points sachants qu'il
    // y a plusieurs segments de type distincts???
    QPointF oldPt{m_fakePoint->previousPoint.x(),
                m_fakePoint->previousPoint.y()};
    QPointF newPt = point;

    //QPointF newPt{m_plot->xAxis->pixelToCoord(point.x()),
    //            m_plot->yAxis->pixelToCoord(point.y())};

    auto pt_it = std::find(abscisse de oldPt)
    m_points.remove();
    m_plot->graph()->removeData(oldPt.x());
    // Restore the previously backed up point ?
    if(m_backedUpPoint != invalid_point)
    {
        m_plot->graph()->addData(m_backedUpPoint.x(), m_backedUpPoint.y());
        m_backedUpPoint = invalid_point;
    }

    // If there is already a point where we move, we have to back it up;
    auto existingVal = m_plot->graph()->data()->value(newPt.x(), QCPData{-1, -1});
    if(existingVal.key != -1)
    {
        m_backedUpPoint = {existingVal.key, existingVal.value};
    }
    else
    {
        m_backedUpPoint = invalid_point;
    }
    m_plot->graph()->removeData(newPt.x());
    m_plot->graph()->addData(newPt.x(), newPt.y());
    m_plot->replot();

    m_fakePoint->previousPoint = newPt;

    m_points->setPoints(pointsToPixels(*m_plot->graph()->data()));
    update();
}

void Curve::removeFakePoint()
{
    this->scene()->removeItem(m_fakePoint);
    delete m_fakePoint;
    m_fakePoint = nullptr;
}

void Curve::pointMoved(const QPointF &pt)
{
    QPointF newPt{m_plot->xAxis->pixelToCoord(pt.x()),
                m_plot->yAxis->pixelToCoord(pt.y())};

    emit pointMovingFinished(m_fakePoint->originalPoint.x(),
                             newPt.x(),
                             1. - newPt.y());
}

void Curve::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    emit mousePressed();
    if(qApp->keyboardModifiers() == Qt::ControlModifier)
    {
        QPointF newPt{m_plot->xAxis->pixelToCoord(event->pos().x()),
                    1. - m_plot->yAxis->pixelToCoord(event->pos().y())};
        emit pointCreated(newPt);
    }
}

void Curve::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    auto point = pointUnderMouse(event);
    if(m_fakePoint)
    {
        removeFakePoint();
    }

    if(point != invalid_point)
    {
        QPointF pixelPt{m_plot->xAxis->coordToPixel(point.x()),
                    m_plot->yAxis->coordToPixel(point.y())};

        m_fakePoint = new MyPoint(point, pixelPt, this);
    }
}

QPointF Curve::pointUnderMouse(QGraphicsSceneMouseEvent* event)
{
    using namespace std;
    double mPosX = m_plot->xAxis->pixelToCoord(event->pos().x());
    double mPosY = m_plot->yAxis->pixelToCoord(event->pos().y());

    QList<QCPData> dl = m_plot->graph()->data()->values();

    double Xoffset = m_plot->xAxis->pixelToCoord(7.); // TODO pen size
    double Yoffset = 1. - m_plot->yAxis->pixelToCoord(7.);

    auto ptIt = find_if(begin(dl), end(dl),
                        [&] (const QCPData& val)
    {
        return (mPosX <= val.key + Xoffset)
                && (mPosX >= val.key - Xoffset)
                && (mPosY <= val.value + Yoffset)
                && (mPosY >= val.value - Yoffset);
    });

    return (ptIt == end(dl)) ? QPointF{-1., -1.} : QPointF{ptIt->key, ptIt->value};
}
*/

CurveSegmentModel *CurveSegmentModel::previous() const
{
    return m_previous;
}

void CurveSegmentModel::setPrevious(CurveSegmentModel *previous)
{
    if(previous != m_previous)
    {
        m_previous = previous;
        emit previousChanged();
    }
}
CurveSegmentModel *CurveSegmentModel::following() const
{
    return m_following;
}

void CurveSegmentModel::setFollowing(CurveSegmentModel *following)
{
    if(following != m_following)
    {
        m_following = following;
        emit followingChanged();
    }
}

