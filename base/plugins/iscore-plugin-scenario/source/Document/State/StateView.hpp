#pragma once
#include <QGraphicsObject>

class QMimeData;
class StatePresenter;

class StateView : public QGraphicsObject
{
        Q_OBJECT
    public:
        StateView(StatePresenter &presenter, QGraphicsItem *parent = 0);
        virtual ~StateView() = default;
        int type() const override;

        const StatePresenter& presenter() const;

        QRectF boundingRect() const override;
        void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

        void setContainMessage(bool);
        void setSelected(bool arg);

        void changeColor(const QColor&);

    signals:
        void dropReceived(const QMimeData*);

    protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

        virtual void dropEvent(QGraphicsSceneDragDropEvent *event) override;
    private:
        StatePresenter& m_presenter;

        bool m_containMessage{false};
        bool m_selected{false};

        QColor m_baseColor;

};
