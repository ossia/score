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
        void zoom(QPointF pos);

    protected:
        virtual void resizeEvent(QResizeEvent* ev) override;
        virtual void scrollContentsBy(int dx, int dy) override;
        virtual void wheelEvent(QWheelEvent* event) override;

    private:
        QBrush m_bg;
        SceneGraduations* m_graduations{};
};
