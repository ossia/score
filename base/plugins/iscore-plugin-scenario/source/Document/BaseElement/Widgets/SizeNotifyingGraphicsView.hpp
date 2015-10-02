#pragma once
#include <QGraphicsView>
#include <QResizeEvent>

#include <QPainter>
#include <QApplication>
#include <QGraphicsItem>
#include <QDebug>
#include "SceneGraduations.hpp"

class SizeNotifyingGraphicsView : public QGraphicsView // TODO rename this class
{
        Q_OBJECT
    public:
        SizeNotifyingGraphicsView(QGraphicsScene* parent);

        void setGrid(QPainterPath&& newGrid);

    signals:
        void sizeChanged(const QSize&);
        void scrolled(int);
        void zoom(QPoint pixDelta, QPointF pos);

    protected:
        virtual void resizeEvent(QResizeEvent* ev) override;
        virtual void scrollContentsBy(int dx, int dy) override;
        virtual void wheelEvent(QWheelEvent* event) override;
        virtual void keyPressEvent(QKeyEvent* event) override;
        virtual void keyReleaseEvent(QKeyEvent* event) override;
        virtual void focusOutEvent(QFocusEvent* event) override;

    private:
        QBrush m_bg;
        bool m_zoomModifier{false};
        SceneGraduations* m_graduations{};
};
