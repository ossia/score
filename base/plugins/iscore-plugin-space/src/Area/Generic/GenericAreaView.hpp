#pragma once
#include <QGraphicsItem>
#include <QPainter>
class RectDevice
{
    public:
        QVector<QRectF> rects;
        void clear()
        {
            rects.clear();
        }

        template<typename... Args>
        void add(Args&&... args) // A rect
        {
            rects.push_back({std::forward<Args>(args)...});
        }
};


namespace Space
{
class GenericAreaView : public QGraphicsItem
{
    public:
        GenericAreaView(QGraphicsItem* parent);

        QVector<QRectF> rects;

        QRectF boundingRect() const override;
        void updateRect(const QRectF&);

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* ,
                   QWidget*) override;

    private:
        QRectF m_rect;
        QColor m_col;
};
}
