#pragma once
#include <Scenario/Document/VerticalExtent.hpp>
#include <Process/Style/ColorReference.hpp>
#include <QtGlobal>
#include <QGraphicsItem>
#include <QPoint>
#include <QRect>
#include <QString>

#include "ExecutionStatus.hpp"
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <iscore_plugin_scenario_export.h>
class QGraphicsSceneDragDropEvent;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class QMimeData;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class ConditionView;
class EventPresenter;
class ISCORE_PLUGIN_SCENARIO_EXPORT EventView final : public QGraphicsObject
{
        Q_OBJECT

    public:
        EventView(EventPresenter& presenter, QGraphicsObject* parent);
        virtual ~EventView() = default;

        static constexpr int static_type()
        { return QGraphicsItem::UserType + ItemType::Event; }
        int type() const override
        { return static_type(); }

        const EventPresenter& presenter() const
        { return m_presenter; }

        QRectF boundingRect() const override
        { return {-3, -10., 6, qreal(m_extent.bottom() - m_extent.top() + 20)};  }

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        void setSelected(bool selected);
        bool isSelected() const;

        void setCondition(const QString& cond);
        bool hasCondition() const;

        void setTrigger(const QString& trig);
        bool hasTrigger() const;

        void setExtent(const VerticalExtent& extent);
        void setExtent(VerticalExtent&& extent);

        void setStatus(ExecutionStatus s);

        void changeColor(ColorRef);

        void setWidthScale(double);
        void changeToolTip(const QString&);

    signals:
        void eventHoverEnter();
        void eventHoverLeave();

        void dropReceived(const QPointF& pos, const QMimeData*);

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

        void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;

        void dropEvent(QGraphicsSceneDragDropEvent *event) override;

    private:
        EventPresenter& m_presenter;
        QString m_condition;
        QString m_trigger;
        ColorRef m_color;

        ExecutionStatusProperty m_status{};
        bool m_selected{};

        VerticalExtent m_extent;

        ConditionView* m_conditionItem{};
};
}
