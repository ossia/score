#pragma once

#include <Process/Style/ColorReference.hpp>
#include <QGraphicsTextItem>

namespace Scenario
{

class TextItem final : public QGraphicsTextItem
{
    Q_OBJECT
    public:
    TextItem(QString text, QGraphicsObject* parent);

    signals:
    void focusOut();

    protected:
    void focusOutEvent(QFocusEvent* event) override;

};

class SimpleTextItem final : public QGraphicsSimpleTextItem
{
    public:
        using QGraphicsSimpleTextItem::QGraphicsSimpleTextItem;

        void paint(
                QPainter *painter,
                const QStyleOptionGraphicsItem *option,
                QWidget *widget) override
        {
            setPen(m_color.getColor());
            setBrush(m_color.getBrush());
            QGraphicsSimpleTextItem::paint(painter, option, widget);
        }

        void setColor(ColorRef c)
        {
            m_color = c;
            update();
        }

    private:
        ColorRef m_color;
};

}
