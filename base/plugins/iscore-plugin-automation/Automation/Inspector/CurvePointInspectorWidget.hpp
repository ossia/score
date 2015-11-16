#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

class CurvePointModel;
class QDoubleSpinBox;

class CurvePointInspectorWidget final : public InspectorWidgetBase
{
    Q_OBJECT
    public:
        explicit CurvePointInspectorWidget(
            const CurvePointModel& model,
            iscore::Document& doc,
            QWidget* parent);

    signals:

    public slots:
        void on_pointChanged();

    private:
        const CurvePointModel& m_model;
        QDoubleSpinBox* m_XBox;
        QDoubleSpinBox* m_YBox;
};
