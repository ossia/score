#pragma once
#include "ProcessInterface/ProcessViewInterface.hpp"

#include <QAction>

class TemporalScenarioView : public ProcessViewInterface
{
        Q_OBJECT

    public:
        TemporalScenarioView(QGraphicsObject* parent);

        virtual ~TemporalScenarioView() = default;

        virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    signals:
        void scenarioPressed(QPointF, QPointF);
        void scenarioReleased(QPointF, QPointF);

        void newSelectionArea(QRectF area);

        void deletePressed();
        void clearPressed();

        void ctrlStateChanged(bool);

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

        virtual void keyPressEvent(QKeyEvent* event) override;
        virtual void keyReleaseEvent(QKeyEvent* event) override;

    private:
        QPointF m_clickedPoint {};
        QRectF m_selectArea;

        QAction* m_clearAction {};

        bool m_lock {};
        bool m_clicked {false};
};
