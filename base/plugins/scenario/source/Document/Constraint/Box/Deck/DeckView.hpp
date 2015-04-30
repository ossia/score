#pragma once
#include <QGraphicsObject>

class DeckPresenter;
class DeckView;
class DeckHandle : public QGraphicsItem
{
    public:
        const DeckView& deckView;
        DeckHandle(const DeckView& deckView,
                   QGraphicsItem* parent);
        static constexpr double handleHeight()
        {
            return 3.;
        }

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

        void setWidth(qreal width);

    private:
        qreal m_width {};
};

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
class DeckOverlay : public QGraphicsItem
{
    public:
        const DeckView& deckView;
        DeckOverlay(DeckView* parent);

        virtual QRectF boundingRect() const override;

        void setHeight(qreal height);
        void setWidth(qreal height);

        virtual void paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget) override;

        virtual void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;

    private:
        DeckHandle* m_handle{};
};

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
