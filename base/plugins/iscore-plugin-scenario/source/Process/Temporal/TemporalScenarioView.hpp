#pragma once
#include "ProcessInterface/ProcessView.hpp"

#include <QAction>
#include <QEvent>

class TemporalScenarioPresenter;
class TemporalScenarioView : public ProcessView
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

        void setPresenter(TemporalScenarioPresenter* pres)
        {
            m_pres = pres;
        }

    signals:
        void scenarioPressed(const QPointF&);
        void scenarioReleased(const QPointF&);
        void scenarioMoved(const QPointF&);

        void clearPressed();
        void escPressed();
        void shiftPressed();
        void shiftReleased();

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
        virtual void keyReleaseEvent(QKeyEvent *event) override;

    private:
        QRectF m_selectArea;

        QAction* m_clearAction {};


        bool m_lock {};
        TemporalScenarioPresenter* m_pres{};
};
