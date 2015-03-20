#include "QCustomPlotCurve.hpp"

#include <QGraphicsProxyWidget>
#include <QPainter>


double clamp(double val, double min, double max)
{
    return val < min ? min : (val > max ? max : val);
}

class MyPoint : public QGraphicsItem
{
    public:
        QPointF originalPoint;
        QPointF previousPoint;

        MyPoint(QPointF coordPos, QPointF pixelPos, QCustomPlotCurve* parent):
            QGraphicsItem{parent},
            originalPoint{coordPos},
            previousPoint{coordPos},
            m_rect{parent->boundingRect()},
            m_pointPos{pixelPos},
            m_curve{parent}
        {
            setZValue(3);

            setAcceptHoverEvents(true);
            setFlags(ItemIsMovable);
            setFlag(ItemSendsGeometryChanges);
        }

        QPointF clampPoint(const QPointF& point)
        {
            QPointF movedPointInCoord = { m_curve->m_plot->xAxis->pixelToCoord(point.x()),
                                          m_curve->m_plot->yAxis->pixelToCoord(point.y())};

            auto newX = clamp(movedPointInCoord.x(), 0.0, 1.0);
            if(originalPoint.x() == 0. || originalPoint.x() == 1.)
            {
                newX = originalPoint.x();
            }
            auto newY = clamp(movedPointInCoord.y(), 0.0, 1.0);

            return QPointF{m_curve->m_plot->xAxis->coordToPixel(newX),
                        m_curve->m_plot->yAxis->coordToPixel(newY)};
        }

        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override
        {
            painter->setPen(Qt::blue);
            painter->setBrush(Qt::green);
            painter->drawEllipse(m_pointPos, 7, 7);

            painter->setPen(QColor(255, 80, 80));
            painter->setBrush(QColor(255, 200, 200));
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
            m_curve->setCurrentPointPos(m_pointPos);
            update();
        }

        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
        {
            m_curve->pointMoved(clampPoint(event->pos()));
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
                m_curve->removeFakePoint();
            }
        }

    private:
        QRectF m_rect;
        QPointF m_pointPos;
        QCustomPlotCurve* m_curve{};
};

class PointsLayer : public QGraphicsItem
{
        QRectF m_rect;
    public:
        PointsLayer(QCustomPlotCurve* parent):
            QGraphicsItem{parent}
        {
            setRect(parent->boundingRect());
        }

        // Points in pixel coordinates
        void setPoints(const QList<QPointF>& points)
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
            painter->setPen(Qt::red);
            painter->setBrush(Qt::white);

            for(auto pt : m_points)
                painter->drawEllipse(pt, 5, 5);
        }

    private:
        QList<QPointF> m_points;
};


static const QPointF invalid_point{-1, -1};

QCustomPlotCurve::QCustomPlotCurve(QGraphicsItem* parent):
    QGraphicsObject{parent}
{
    // QCustomPlot
    auto widg = new QGraphicsProxyWidget{this};
    widg->setPos(0, 0);
    widg->setZValue(1);

    m_plot = new QCustomPlot;

    m_plot->axisRect()->setAutoMargins(QCP::msNone);
    m_plot->axisRect()->setMargins(QMargins(0,0,0,0));

    m_plot->setBackground(Qt::NoBrush);

    connect(m_plot, &QCustomPlot::mouseMove,
            this,   &QCustomPlotCurve::on_mouseMoveEvent);
    connect(m_plot, &QCustomPlot::mousePress,
            this,   &QCustomPlotCurve::on_mousePressEvent);

    widg->setWidget(m_plot);

    // The static points
    m_points = new PointsLayer{this};
    m_points->setPos(0,0);
}

void QCustomPlotCurve::setPoints(QList<QPointF> list)
{
    // QCustomPlot
    QVector<double> x, y;

    for(const auto& pt : list)
    {
        x.push_back(pt.x());
        y.push_back(1. - pt.y());

    }

    m_plot->removeGraph(0);
    auto graph = m_plot->addGraph();
    graph->setData(x, y);
    graph->setPen(QPen(QColor(200, 30, 0), 3));
    graph->setLineStyle(QCPGraph::lsLine);

    m_plot->xAxis->setAutoTicks(false);
    m_plot->yAxis->setAutoTicks(false);
    m_plot->xAxis->setRange(0, 1);
    m_plot->yAxis->setRange(0, 1);

    m_plot->replot();

    // The static points
    m_points->setPoints(pointsToPixels(*m_plot->graph()->data()));

    // Cleanup
    if(m_fakePoint)
    {
        removeFakePoint();
    }

    m_backedUpPoint = invalid_point;

    update();
}

void QCustomPlotCurve::setSize(const QSizeF& size)
{
    prepareGeometryChange();
    if(size.width() == 0 || size.height() == 0)
        return;

    m_size = size;

    m_plot->setMinimumSize(m_size.toSize());
    m_plot->setMaximumSize(m_size.toSize());

    if(m_plot && m_plot->graph())
        m_points->setPoints(pointsToPixels(*m_plot->graph()->data()));

    update();
}

QList<QPointF> QCustomPlotCurve::pointsToPixels(const QCPDataMap& data)
{
    QList<QPointF> mappedPoints;
    for(const auto& pt : data.values())
        mappedPoints.push_back({m_plot->xAxis->coordToPixel(pt.key),
                                m_plot->yAxis->coordToPixel(pt.value)});

    return mappedPoints;
}

QDebug& operator<<(QDebug& d, const QCPData& data)
{
    return d << "(" << data.key << ", " << data.value << ")";
}

void QCustomPlotCurve::setCurrentPointPos(QPointF point)
{
    QPointF oldPt{m_fakePoint->previousPoint.x(),
                m_fakePoint->previousPoint.y()};
    QPointF newPt{m_plot->xAxis->pixelToCoord(point.x()),
                m_plot->yAxis->pixelToCoord(point.y())};

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

void QCustomPlotCurve::removeFakePoint()
{
    this->scene()->removeItem(m_fakePoint);
    delete m_fakePoint;
    m_fakePoint = nullptr;
}

void QCustomPlotCurve::pointMoved(QPointF pt)
{
    QPointF newPt{m_plot->xAxis->pixelToCoord(pt.x()),
                m_plot->yAxis->pixelToCoord(pt.y())};

    emit pointMovingFinished(m_fakePoint->originalPoint.x(),
                             newPt.x(),
                             1. - newPt.y());
}

#include <QApplication>
void QCustomPlotCurve::on_mousePressEvent(QMouseEvent* event)
{
    if(qApp->keyboardModifiers() == Qt::ControlModifier)
    {
        QPointF newPt{m_plot->xAxis->pixelToCoord(event->pos().x()),
                    1. - m_plot->yAxis->pixelToCoord(event->pos().y())};
        emit pointCreated(newPt);
    }
}

void QCustomPlotCurve::on_mouseMoveEvent(QMouseEvent* event)
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

QPointF QCustomPlotCurve::pointUnderMouse(QMouseEvent* event)
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


