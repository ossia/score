#pragma once
#include <Process/LayerView.hpp>
#include <qpoint.h>
#include <qrect.h>

class QGraphicsItem;
class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneDragDropEvent;
class QGraphicsSceneMouseEvent;
class QKeyEvent;
class QMimeData;
class QPainter;
class TemporalScenarioPresenter;

class TemporalScenarioView final : public LayerView
{
        Q_OBJECT

    public:
        TemporalScenarioView(QGraphicsItem* parent);
        ~TemporalScenarioView();

        void paint_impl(QPainter* painter) const override;

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
        void pressed(const QPointF&);
        void released(const QPointF&);
        void moved(const QPointF&);

        void clearPressed();
        void escPressed();

        void keyPressed(int);
        void keyReleased(int);

        // Screen pos, scene pos
        void askContextMenu(const QPoint&, const QPointF&);
        void dropReceived(const QPointF& pos, const QMimeData*);


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

        void dropEvent(QGraphicsSceneDragDropEvent *event) override;
    private:
        QRectF m_selectArea;

        bool m_lock {};
        TemporalScenarioPresenter* m_pres{};
};
