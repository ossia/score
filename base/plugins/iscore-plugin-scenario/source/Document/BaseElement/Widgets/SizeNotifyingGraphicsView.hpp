#pragma once
#include <QGraphicsView>
#include <QResizeEvent>

#include <QPainter>
#include <QOpenGLWidget>
#include <QApplication>
class SizeNotifyingGraphicsView : public QGraphicsView
{
        Q_OBJECT
    public:
        SizeNotifyingGraphicsView(QGraphicsScene* parent):
            QGraphicsView{parent}
        {
            this->setCacheMode(QGraphicsView::CacheBackground);
            /*
            setViewport(new QOpenGLWidget);
            //setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
            setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            setAlignment(Qt::AlignTop | Qt::AlignLeft);
            */
        }

        void setGrid(QPainterPath&& newGrid)
        {
            m_grid = std::move(newGrid);
            update();
        }

    signals:
        void sizeChanged(const QSize&);
        void scrolled(const int);

    protected:
        virtual void resizeEvent(QResizeEvent* ev) override
        {
            QGraphicsView::resizeEvent(ev);
            emit sizeChanged(size());
        }
        virtual void scrollContentsBy(int dx, int dy) override
        {
            QGraphicsView::scrollContentsBy(dx, dy);
            emit scrolled(dx);
        }

        virtual void drawBackground(QPainter * painter, const QRectF & rect) override
        {
            painter->fillRect(rect, qApp->palette("ScenarioPalette").background());
            painter->setPen(qApp->palette("ScenarioPalette").background().color().lighter());
            painter->drawPath(m_grid);
        }


    private:
        QPainterPath m_grid;
        const QBrush m_bgBrush{Qt::lightGray};
        const QPen m_bgPen{QBrush(Qt::gray), 1, Qt::DashLine};
};
