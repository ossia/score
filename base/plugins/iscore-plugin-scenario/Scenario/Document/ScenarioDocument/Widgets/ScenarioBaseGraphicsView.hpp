#pragma once


#include <QGraphicsView>
#include <QPoint>

class QFocusEvent;
class QGraphicsScene;
class QKeyEvent;
class QPainterPath;
class QResizeEvent;
class QSize;
class QWheelEvent;
class SceneGraduations;

class ScenarioBaseGraphicsView final : public QGraphicsView
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
        void resizeEvent(QResizeEvent* ev) override;
        void scrollContentsBy(int dx, int dy) override;
        void wheelEvent(QWheelEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;

    private:
        bool m_zoomModifier{false};
        SceneGraduations* m_graduations{};
};
