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

class AreaView : public QGraphicsItem
{
    public:
        AreaView(QGraphicsItem* parent);

        QVector<QRectF> rects;

        QRectF boundingRect() const;
        void updateRect(const QRectF&);

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* ,
                   QWidget*);

    private:
        QRectF m_rect;
};
