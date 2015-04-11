#pragma once
#include <QGraphicsView>
#include <QResizeEvent>

#include <QPainter>
#include <QOpenGLWidget>
class SizeNotifyingGraphicsView : public QGraphicsView
{
        Q_OBJECT
    public:
        SizeNotifyingGraphicsView(QGraphicsScene* parent):
            QGraphicsView{parent}
        {
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
            painter->fillRect(rect, m_bgBrush);
            painter->setPen(m_bgPen);
            painter->drawPath(m_grid);
        }


    private:
        QPainterPath m_grid;
        const QBrush m_bgBrush{Qt::lightGray};
        const QPen m_bgPen{QBrush(Qt::gray), 1, Qt::DashLine};
};
