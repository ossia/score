#pragma once
#include <QGraphicsObject>

class DeckOverlay;
class DeckPresenter;
class DeckView : public QGraphicsObject
{
        Q_OBJECT

    public:
        const DeckPresenter& presenter;

        DeckView(const DeckPresenter&pres, QGraphicsObject* parent);
        virtual ~DeckView() = default;

        virtual QRectF boundingRect() const override;
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override;

        static constexpr double handleHeight()
        {
            return 5.;
        }

        void setHeight(qreal height);
        qreal height() const;

        void setWidth(qreal width);
        qreal width() const;

        void enable();
        void disable();

        void setFocus(bool b);

    private:
        qreal m_height {};
        qreal m_width {};
        DeckOverlay* m_overlay{};
        bool m_focus{false};
};

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
class DeckOverlay : public QGraphicsItem
{
    public:
        const DeckView& deckView;
        DeckOverlay(DeckView* parent):
            QGraphicsItem{parent},
            deckView{*parent}
        {
            this->setZValue(1000);
            this->setPos(0, 0);
        }

        virtual QRectF boundingRect() const override;

        virtual void paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget) override;

        virtual void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
};
