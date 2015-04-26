#pragma once
#include "ProcessInterface/ProcessViewInterface.hpp"

#include <QAction>
#include <QEvent>

class TemporalScenarioView : public ProcessViewInterface
{
        Q_OBJECT

    public:
        TemporalScenarioView(QGraphicsObject* parent);
        ~TemporalScenarioView();

        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

        void setSelectionArea(const QRectF& rect)
        {
            m_selectArea = rect;
            update();
        }

    signals:
        void scenarioPressed(const QPointF&);
        void scenarioReleased(const QPointF&);
        void scenarioMoved(const QPointF&);

        void clearPressed();
        void escPressed();

    public slots:
        void lock()
        {
            m_lock = true;
            update();
        }
        void unlock()
        {
            m_lock = false;
            update();
        }

    protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
        virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

        virtual void keyPressEvent(QKeyEvent *event) override;

    private:
        QRectF m_selectArea;

        QAction* m_clearAction {};

        bool m_lock {};
};
