#pragma once
#include <QGraphicsView>
#include <QResizeEvent>

#include <QPainter>
#include <QApplication>
#include <QGraphicsItem>
#include <QDebug>
#include "SceneGraduations.hpp"

class ScenarioBaseGraphicsView : public QGraphicsView
{
        Q_OBJECT
    public:
        ScenarioBaseGraphicsView(QGraphicsScene* parent);

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
        bool m_zoomModifier{false};
        SceneGraduations* m_graduations{};
};
