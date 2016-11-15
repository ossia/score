#pragma once
#include <iscore/model/ColorReference.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include <Scenario/Document/Constraint/ExecutionState.hpp>
#include <Scenario/Document/CommentBlock/TextItem.hpp>
#include <Process/TimeValue.hpp>
#include <QColor>
#include <QtGlobal>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QPainter>

class QMimeData;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneDragDropEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class TemporalConstraintPresenter;
class ConstraintDurations;

class ISCORE_PLUGIN_SCENARIO_EXPORT TemporalConstraintView final :
        public ConstraintView
{
        Q_OBJECT

    public:
        TemporalConstraintView(
                TemporalConstraintPresenter& presenter,
                QGraphicsItem* parent);

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

        void setLabelColor(iscore::ColorRef labelColor);
        void setLabel(const QString &label);

        void setFocused(bool b)
        {
            m_hasFocus = b;
            update();
        }

        void setColor(iscore::ColorRef c)
        {
            m_bgColor = c;
            update();
        }

        void setExecutionState(ConstraintExecutionState);
        void setExecutionDuration(const TimeValue& progress);

    signals:
        void constraintHoverEnter();
        void constraintHoverLeave();
        void dropReceived(const QPointF& pos, const QMimeData*);

    protected:
        void hoverEnterEvent(QGraphicsSceneHoverEvent* h) override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent* h) override;
        void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
        void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) override;
        void dropEvent(QGraphicsSceneDragDropEvent *event) override;

    private:
        QString m_label{};

        iscore::ColorRef m_bgColor;
        SimpleTextItem* m_labelItem{};
        SimpleTextItem* m_counterItem{};

        bool m_shadow {false};
        bool m_hasFocus{};
        ConstraintExecutionState m_state{};

};
}
