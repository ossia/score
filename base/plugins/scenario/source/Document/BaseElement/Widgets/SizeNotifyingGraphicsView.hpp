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

    protected:
        virtual void resizeEvent(QResizeEvent* ev)
        {
            QGraphicsView::resizeEvent(ev);
            emit sizeChanged(size());
        }
};
