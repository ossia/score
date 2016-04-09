#pragma once
#include <Process/Style/ColorReference.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include <Scenario/Document/Constraint/ExecutionState.hpp>
#include <QColor>
#include <QtGlobal>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QPainter>

class QGraphicsObject;
class QGraphicsSceneHoverEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class TemporalConstraintPresenter;

class LeftBraceView;
class RightBraceView;

// MOVEME
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
class ISCORE_PLUGIN_SCENARIO_EXPORT TemporalConstraintView final : public ConstraintView
{
        Q_OBJECT

    public:
        TemporalConstraintView(
                TemporalConstraintPresenter& presenter,
                QGraphicsObject* parent);

        QRectF boundingRect() const override
        {
            qreal x = std::min(0., minWidth());
            qreal rectW = infinite() ? defaultWidth() : maxWidth();
            rectW -= x;
            return {x, -4, rectW, qreal(constraintAndRackHeight()) };
        }

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        bool shadow() const;
        void setShadow(bool shadow);

        void setLabelColor(ColorRef labelColor);
        void setLabel(const QString &label);

        void setFocused(bool b)
        {
            m_hasFocus = b;
            update();
        }

        void setColor(ColorRef c)
        {
            m_bgColor = c;
            update();
        }

        void setHeightScale(double);
        void setExecutionState(ConstraintExecutionState);

    signals:
        void constraintHoverEnter();
        void constraintHoverLeave();

    protected:
        void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;

    private:
        QPointF m_clickedPoint {};

        bool m_shadow {false};
        bool m_hasFocus{};
        QString m_label{};
        ColorRef m_bgColor;

        LeftBraceView* m_leftBrace{};
        RightBraceView* m_rightBrace{};

        ConstraintExecutionState m_state{};
        SimpleTextItem* m_labelItem{};
        SimpleTextItem* m_counterItem{};
};
}
