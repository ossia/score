#pragma once
#include <QGraphicsItem>
#include <QList>
#include <QRect>

#include <iscore/tools/ObjectPath.hpp>

class ConstraintModel;
class ClickableLabelItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class AddressBarItem final : public QGraphicsObject
{
        Q_OBJECT
    public:
        AddressBarItem(QGraphicsItem* parent);

        double width() const;
        void setTargetObject(ObjectPath&&);

        QRectF boundingRect() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    signals:
        void needRedraw();
        void constraintSelected(ConstraintModel& cst);

    private:
        void redraw();
        QList<QGraphicsItem*> m_items;
        ObjectPath m_currentPath;

        double m_width{};
};
