#pragma once
#include <QGraphicsTextItem>
#include <QGraphicsObject>
#include <functional>

class SeparatorItem : public QGraphicsSimpleTextItem
{
    public:
        SeparatorItem(QGraphicsItem* parent);
};

class ClickableLabelItem : public QGraphicsSimpleTextItem
{
    public:
        using ClickHandler= std::function<void(ClickableLabelItem*)>;
        ClickableLabelItem(
                ClickHandler&& onClick,
                const QString& text,
                QGraphicsItem* parent);

        int index() const;
        void setIndex(int index);

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event);
        void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
        void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    private:
        int m_index{-1};
        ClickHandler m_onClick;
};
