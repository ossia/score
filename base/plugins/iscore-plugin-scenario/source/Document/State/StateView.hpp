#pragma once
#include <QGraphicsObject>

class DisplayedStateModel;

class StateView : public QGraphicsObject
{
        Q_OBJECT
    public:
        StateView(DisplayedStateModel &model, QGraphicsObject *parent = 0);
        virtual ~StateView() = default;
        int type() const override;

        const DisplayedStateModel& model() const;

        QRectF boundingRect() const override;
        void paint(QPainter* painter,
               const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

        void setContainMessage(bool);
        void setSelected(bool arg)
        {
            m_selected = arg;
            update();
        }

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

    public slots:

    protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        DisplayedStateModel& m_model;

        bool m_containMessage{false};
        bool m_selected{false};

};
