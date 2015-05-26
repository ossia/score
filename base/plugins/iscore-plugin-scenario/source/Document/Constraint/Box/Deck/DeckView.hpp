#pragma once
#include <QGraphicsObject>

class DeckPresenter;
class DeckOverlay;
class DeckHandle;

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
        DeckHandle* m_handle{};
        bool m_focus{false};
};
