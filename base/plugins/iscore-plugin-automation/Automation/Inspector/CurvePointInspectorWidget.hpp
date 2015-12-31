#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>

class QDoubleSpinBox;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore
namespace Curve
{
class PointModel;
}
// TODO automation instead.
class CurvePointInspectorWidget final : public InspectorWidgetBase
{
    public:
        explicit CurvePointInspectorWidget(
            const Curve::PointModel& model,
            const iscore::DocumentContext& context,
            QWidget* parent);

    private:
        void on_pointChanged(double);
        void on_editFinished();

        const Curve::PointModel& m_model;
        QDoubleSpinBox* m_XBox;
        QDoubleSpinBox* m_YBox;
        OngoingCommandDispatcher m_dispatcher;
        double m_yFactor;
        double m_xFactor;

        double m_Ymin;
};
