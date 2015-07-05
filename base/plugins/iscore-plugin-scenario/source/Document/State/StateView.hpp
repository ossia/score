#pragma once
#include <QGraphicsObject>

class StatePresenter;

class StateView : public QGraphicsObject
{
        Q_OBJECT
    public:
        StateView(StatePresenter &presenter, QGraphicsObject *parent = 0);
        virtual ~StateView() = default;
        int type() const override;

        const StatePresenter& presenter() const;

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

        void changeColor(const QColor&)
        {
            qDebug() << "TODO: " << Q_FUNC_INFO;
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
        StatePresenter& m_presenter;

        bool m_containMessage{false};
        bool m_selected{false};

};
