#pragma once
#include <QGraphicsView>
#include <QResizeEvent>

#include <QPainter>
#include <QOpenGLWidget>
#include <QApplication>
#include <QGraphicsItem>
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

    protected:
        virtual void resizeEvent(QResizeEvent* ev) override;
        virtual void scrollContentsBy(int dx, int dy) override;

    private:
        QBrush m_bg;
        SceneGraduations* m_graduations{};
};
