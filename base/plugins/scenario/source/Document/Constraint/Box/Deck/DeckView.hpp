#pragma once
#include <QGraphicsObject>

class DeckView : public QGraphicsObject
{
        Q_OBJECT

    public:
        DeckView(QGraphicsObject* parent);
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
};

