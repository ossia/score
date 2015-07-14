#include "AddressBarItem.hpp"
#include "ClickableLabelItem.hpp"

#include <QGraphicsLayout>
#include <QPainter>
AddressBarItem::AddressBarItem(QGraphicsItem *parent):
    QGraphicsObject{parent}
{

}

void AddressBarItem::setTargetObject(ObjectPath && path)
{
    qDeleteAll(m_items);
    m_items.clear();

    m_currentPath = std::move(path);

    double currentWidth = 0;
    int i = -1;
    for(auto& identifier : m_currentPath)
    {
        i++;
        if(identifier.objectName() != "BaseConstraintModel"
                && identifier.objectName() != "ConstraintModel")
            continue;

        QString txt = QString{"%1%2"}
                .arg(identifier.objectName())
                .arg(identifier.id()
                     ? "." + QString::number(*identifier.id())
                     : "");

        auto lab = new ClickableLabelItem{
                [&] (ClickableLabelItem* item) { on_elementClicked(item); },
                txt,
                this};

        lab->setIndex(i);

        m_items.append(lab);
        lab->setPos(currentWidth, 0);
        currentWidth += 10 + lab->boundingRect().width();

        auto sep = new SeparatorItem{this};
        sep->setPos(currentWidth, 0);
        currentWidth += 10 + sep->boundingRect().width();
        m_items.append(sep);
    }

    prepareGeometryChange();
    m_width = currentWidth;
}

void AddressBarItem::on_elementClicked(ClickableLabelItem * clicked)
{
    int index = clicked->index();

    if(index < m_currentPath.vec().size())
    {
        auto vec = m_currentPath.vec();
        vec.resize(index + 1);

        emit objectSelected(std::move(vec));
    }
}
double AddressBarItem::width() const
{
    return m_width;
}



QRectF AddressBarItem::boundingRect() const
{
    return {};
}

void AddressBarItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
}
