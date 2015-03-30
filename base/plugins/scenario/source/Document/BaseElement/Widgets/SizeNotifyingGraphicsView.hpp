#pragma once
#include <QGraphicsView>
#include <QResizeEvent>

#include <QPainter>

class SizeNotifyingGraphicsView : public QGraphicsView
{
        Q_OBJECT
    public:
        using QGraphicsView::QGraphicsView;

        void setGrid(QPainterPath newGrid)
        {
            m_grid = newGrid;
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
            painter->fillRect(rect, QBrush{Qt::lightGray});
            painter->setPen(QPen(QBrush(Qt::gray), 1, Qt::DashLine));
            painter->drawPath(m_grid);
        }
        QPainterPath m_grid;
};
