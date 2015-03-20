#include "QCustomPlotCurve.hpp"

#include <qcustomplot.h>
#include <QGraphicsProxyWidget>
#include <QPainter>


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

        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override
        {
            painter->setPen(Qt::blue);
            painter->setBrush(Qt::green);
            painter->drawEllipse(m_pointPos, 5, 5);

            painter->setPen(QColor(255, 80, 80));
            painter->setBrush(QColor(255, 200, 200));
            painter->drawEllipse(m_pointPos, 3, 3);
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
            m_pointPos = event->pos();
            m_curve->setCurrentPointPos(m_pointPos);
            update();
        }

        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
        {
            m_curve->pointMoved(event->pos());
        }

        void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override
        {
            using namespace std;
            double x = event->pos().x();
            double y = event->pos().y();

            if(!((x <= m_pointPos.x() + 5)
            && (x >= m_pointPos.x() - 5)
            && (y <= m_pointPos.y() + 5)
            && (y >= m_pointPos.y() - 5)))
            {
                m_curve->removeFakePoint();
            }
        }

    private:
        QRectF m_rect;
        QPointF m_pointPos;
        QCustomPlotCurve* m_curve{};
};




static const QPointF invalid_point{-1, -1};

QCustomPlotCurve::QCustomPlotCurve(QGraphicsItem* parent):
    QGraphicsObject{parent}
{
    auto widg = new QGraphicsProxyWidget{this};
    widg->setPos(0, 0);
    widg->setZValue(2);

    m_plot = new QCustomPlot;

    m_plot->axisRect()->setAutoMargins(QCP::msNone);
    m_plot->axisRect()->setMargins(QMargins(0,0,0,0));

    m_plot->setBackground(Qt::NoBrush);

    connect(m_plot, &QCustomPlot::mouseMove,
            this,   &QCustomPlotCurve::on_mouseMoveEvent);

    widg->setWidget(m_plot);
}

void QCustomPlotCurve::setPoints(QList<QPointF> list)
{
    QVector<double> x, y;
    for(auto pt : list)
    {
        x.push_back(pt.x());
        y.push_back(1. - pt.y());
    }

    m_plot->removeGraph(0);
    auto graph = m_plot->addGraph();
    graph->setData(x, y);
    graph->setPen(QPen(QColor(200, 30, 0), 3));
    graph->setLineStyle(QCPGraph::lsLine);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));

    m_plot->xAxis->setAutoTicks(false);
    m_plot->yAxis->setAutoTicks(false);
    m_plot->xAxis->setRange(0, 1);
    m_plot->yAxis->setRange(0, 1);

    m_plot->replot();

    if(m_fakePoint)
    {
        removeFakePoint();
    }
}

void QCustomPlotCurve::setSize(const QSizeF& size)
{
    prepareGeometryChange();
    if(size.width() == 0 || size.height() == 0)
        return;

    m_size = size;

    m_plot->setMinimumSize(m_size.toSize());
    m_plot->setMaximumSize(m_size.toSize());

    update();
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
    m_plot->graph()->addData(newPt.x(), newPt.y());
    m_plot->replot();

    m_fakePoint->previousPoint = newPt;
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

    double Xoffset = m_plot->xAxis->pixelToCoord(5); // TODO pen size
    double Yoffset = 1. - m_plot->yAxis->pixelToCoord(5);

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
