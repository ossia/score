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

        void keyPressed(int);
        void keyReleased(int);

        void askContextMenu(const QPoint&);

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
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

        void keyPressEvent(QKeyEvent *event) override;
        void keyReleaseEvent(QKeyEvent *event) override;

    private:
        QRectF m_selectArea;

        bool m_lock {};
        TemporalScenarioPresenter* m_pres{};
};
