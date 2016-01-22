#pragma once
#include <QBrush>
#include <QGraphicsItem>
#include <qnamespace.h>
#include <iscore_plugin_scenario_export.h>

#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>

class QGraphicsSceneMouseEvent;

namespace Scenario
{

class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintBrace : public QGraphicsObject
{
    Q_OBJECT
    public:
      ConstraintBrace(const TemporalConstraintView& parentCstr, QGraphicsItem* parent);

      QRectF boundingRect() const override;

      void paint(QPainter* painter,
                 const QStyleOptionGraphicsItem* option,
                 QWidget* widget) override;

    signals:

    private:
        const TemporalConstraintView& m_parent;
        QPainterPath m_path;

};

}
