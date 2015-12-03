#pragma once
#include <QtGlobal>
#include <QGraphicsItem>
#include <QPoint>
#include <QRect>
#include <QString>

class QGraphicsSceneContextMenuEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class SlotHandle;
class SlotOverlay;
class SlotPresenter;

class SlotView final : public QGraphicsObject
{
        Q_OBJECT

    public:
        const SlotPresenter& presenter;

        SlotView(const SlotPresenter&pres, QGraphicsObject* parent);
        virtual ~SlotView() = default;

        QRectF boundingRect() const override;
        void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override;

        void setHeight(qreal height);
        qreal height() const;

        void setWidth(qreal width);
        qreal width() const;

        void enable();
        void disable();

        void setFocus(bool b);

        void setFrontProcessName(const QString&);

    signals:
        void askContextMenu(const QPoint&, const QPointF&);

    private:
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

        qreal m_height {};
        qreal m_width {};
        SlotOverlay* m_overlay{};
        SlotHandle* m_handle{};
        bool m_focus{false};
        QString m_frontProcessName{};
};
