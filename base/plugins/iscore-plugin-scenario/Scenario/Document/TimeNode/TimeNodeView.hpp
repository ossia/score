#pragma once
#include <Scenario/Document/VerticalExtent.hpp>
#include <iscore/model/ColorReference.hpp>
#include <QColor>
#include <QGraphicsItem>
#include <QPoint>
#include <QRect>
#include <QTextLayout>
#include <iscore_plugin_scenario_export.h>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

#include <Scenario/Document/CommentBlock/TextItem.hpp>
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class TimeNodePresenter;

class ISCORE_PLUGIN_SCENARIO_EXPORT TimeNodeView final : public QGraphicsItem
{
    public:
        TimeNodeView(TimeNodePresenter& presenter,
                     QGraphicsItem* parent);
        ~TimeNodeView();

        static constexpr int static_type()
        { return QGraphicsItem::UserType + ItemType::TimeNode; }
        int type() const override
        { return static_type(); }

        const TimeNodePresenter& presenter() const
        { return m_presenter;}

        void paint(QPainter* painter,
                   const QStyleOptionGraphicsItem* option,
                   QWidget* widget) override;

        // QGraphicsItem interface
        QRectF boundingRect() const override
        { return { -3., 0., 6., m_extent.bottom() - m_extent.top()}; }

        void setExtent(const VerticalExtent& extent);
        void setExtent(VerticalExtent&& extent);
        void addPoint(int newY);

        void setMoving(bool);
        void setSelected(bool selected);

        bool isSelected() const
        {
            return m_selected;
        }

        void changeColor(iscore::ColorRef);
        void setLabel(const QString& label);

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        TimeNodePresenter& m_presenter;
        VerticalExtent m_extent;

        QPointF m_clickedPoint {};
        iscore::ColorRef m_color;
        bool m_selected{};

        SimpleTextItem* m_text{};
};
}
