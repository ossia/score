#pragma once
#include <QGraphicsView>
#include <QResizeEvent>

class SizeNotifyingGraphicsView : public QGraphicsView
{
        Q_OBJECT
    public:
        using QGraphicsView::QGraphicsView;

    signals:
        void sizeChanged(const QSize&);
        void scrolled(const int);

    protected:
        virtual void resizeEvent(QResizeEvent* ev)
        {
            QGraphicsView::resizeEvent(ev);
            emit sizeChanged(size());
        }
        virtual void scrollContentsBy(int dx, int dy) override
        {
            QGraphicsView::scrollContentsBy(dx, dy);
            emit scrolled(dx);
        }
};
