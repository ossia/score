#pragma once
#include <QGraphicsItem>
#include <QString>
#include <functional>

class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

class SeparatorItem final : public QGraphicsSimpleTextItem
{
    public:
        SeparatorItem(QGraphicsItem* parent);
};

class ClickableLabelItem final : public QGraphicsSimpleTextItem
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
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    private:
        int m_index{-1};
        ClickHandler m_onClick;
};
