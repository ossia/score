#pragma once
#include <Process/Style/ColorReference.hpp>
#include <QColor>
#include <QPen>
#include <QGraphicsTextItem>
#include <QGraphicsSimpleTextItem>

namespace Scenario
{
// TODO move these two
class TextItem final : public QGraphicsTextItem
{
    Q_OBJECT
    public:
    TextItem(QString text, QGraphicsItem* parent);

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
                QWidget *widget) override;

        void setColor(ColorRef c)
        {
            m_color = c;
            update();
        }

    private:
        ColorRef m_color;
};


}
