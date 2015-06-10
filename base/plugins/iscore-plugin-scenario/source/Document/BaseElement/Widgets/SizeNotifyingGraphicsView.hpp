#pragma once
#include <QGraphicsView>
#include <QResizeEvent>

#include <QPainter>
#include <QOpenGLWidget>
#include <QApplication>
#include <QGraphicsItem>
#include <QDebug>
#include "SceneGraduations.hpp"

class SizeNotifyingGraphicsView : public QGraphicsView
{
        Q_OBJECT
    public:
        SizeNotifyingGraphicsView(QGraphicsScene* parent);

        void setGrid(QPainterPath&& newGrid);

    signals:
        void sizeChanged(const QSize&);
        void scrolled(int);
        void zoom(QPointF pos, QPoint pixDelta);

    protected:
        virtual void resizeEvent(QResizeEvent* ev) override;
        virtual void scrollContentsBy(int dx, int dy) override;
        virtual void wheelEvent(QWheelEvent* event) override;
        virtual void keyPressEvent(QKeyEvent* event) override;
        virtual void keyReleaseEvent(QKeyEvent* event) override;

    private:
        QBrush m_bg;
        bool m_zoomModifier{false};
        SceneGraduations* m_graduations{};
};
