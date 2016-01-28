#pragma once
#include <QGraphicsItem>
#include <QString>
#include <functional>
class ModelMetadata;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

namespace Scenario
{
class SeparatorItem final :
        public QGraphicsSimpleTextItem
{
    public:
        SeparatorItem(QGraphicsItem* parent);
};

class ClickableLabelItem final :
        public QObject,
        public QGraphicsSimpleTextItem
{
        Q_OBJECT
    public:
        using ClickHandler= std::function<void(ClickableLabelItem*)>;
        ClickableLabelItem(
                ModelMetadata& constraint,
                ClickHandler&& onClick,
                const QString& text,
                QGraphicsItem* parent);

        int index() const;
        void setIndex(int index);

    signals:
        void textChanged();

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    private:
        int m_index{-1};
        ClickHandler m_onClick;
};
}
