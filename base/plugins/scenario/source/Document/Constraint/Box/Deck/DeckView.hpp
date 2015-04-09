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

        static constexpr int borderHeight()
        {
            return 5;
        }

        void setHeight(int height);
        int height() const;

        void setWidth(int width);
        int width() const;

        void enable();
        void disable();

    signals:
        void bottomHandleSelected();
        void bottomHandleChanged(int newHeight);
        void bottomHandleReleased();

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    private:
        int m_height {};
        int m_width {};
        DeckOverlay* m_overlay{};
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

        virtual QRectF boundingRect() const override
        {
            return deckView.boundingRect();
        }

        virtual void paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget) override
        {
            painter->setBrush(QColor(200, 200, 200, 200));
            painter->drawRect(boundingRect());
        }

        virtual void mousePressEvent(QGraphicsSceneMouseEvent* ev)
        {
            ev->ignore();
        }
};
