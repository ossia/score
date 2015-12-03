#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include <QColor>
#include <QtGlobal>
#include <QPoint>
#include <QRect>
#include <QString>

class QGraphicsObject;
class QGraphicsSceneHoverEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class TemporalConstraintPresenter;

class TemporalConstraintView final : public ConstraintView
{
        Q_OBJECT

    public:
        TemporalConstraintView(TemporalConstraintPresenter& presenter,
                               QGraphicsObject* parent);

        QRectF boundingRect() const override
        { return {0, -15, qreal(maxWidth()), qreal(constraintHeight()) }; }

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        bool shadow() const;
        void setShadow(bool shadow);

        void setLabelColor(const QColor &labelColor);
        void setLabel(const QString &label);

        void setFocused(bool b)
        {
            m_hasFocus = b;
            m_bgColor.setAlpha(m_hasFocus ? 84 : 76);
            update();
        }

        void setColor(QColor c)
        {
            m_bgColor = c;
            m_bgColor.setAlpha(m_hasFocus ? 84 : 76);
            update();
        }

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
        QColor m_labelColor;
        QColor m_bgColor;
};
