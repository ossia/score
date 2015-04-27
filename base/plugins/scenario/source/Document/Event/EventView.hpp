#pragma once
#include <QGraphicsObject>
#include <QMouseEvent>
#include <QKeyEvent>
class EventPresenter;
class EventView : public QGraphicsObject
{
        Q_OBJECT

    public:
        EventView(EventPresenter& presenter, QGraphicsObject* parent);
        virtual ~EventView() = default;

        int type() const override;

        const EventPresenter& presenter() const;

        QRectF boundingRect() const override;
        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        void setSelected(bool selected);

        bool isSelected() const;

        bool isShadow() const;

    signals:
        void eventHoverEnter();
        void eventHoverLeave();

    public slots:
        void changeColor(QColor);
        void setShadow(bool arg);

    protected:
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

        virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;

    private:
        EventPresenter& m_presenter;
        QPointF m_clickedPoint {};
        QColor m_color;

        bool m_shadow {false};
        bool m_selected{};
};

