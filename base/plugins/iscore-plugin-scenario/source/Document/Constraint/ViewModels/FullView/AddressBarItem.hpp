#pragma once
#include <QGraphicsObject>
#include <QList>
#include <iscore/tools/ObjectPath.hpp>

class ClickableLabelItem;
class AddressBarItem : public QGraphicsObject
{
        Q_OBJECT
    public:
        AddressBarItem(QGraphicsItem* parent);

        double width() const;
        void setTargetObject(ObjectPath&&);

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    signals:
        void objectSelected(ObjectPath path);

    private:
        void on_elementClicked(ClickableLabelItem*);

        QList<QGraphicsItem*> m_items;
        ObjectPath m_currentPath;

        double m_width{};
};
