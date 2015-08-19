#pragma once
#include "src/Space/SpaceModel.hpp"
#include "src/Space/ViewportModel.hpp"
#include <QtWidgets>
class ViewportEditWidget : public QWidget
{
    public:
        ViewportEditWidget(const SpaceModel& sp, const ViewportModel& vp, QWidget* parent);

    private:
        const ViewportModel& m_viewport;

        QLineEdit* m_name{};
        QDoubleSpinBox* m_zoom{};
        QDoubleSpinBox* m_rotation{};

        QDoubleSpinBox* m_topleftX{};
        QDoubleSpinBox* m_topleftY{};

        QComboBox* m_dimX{};
        QComboBox* m_dimY{};

        QVector<QComboBox*> m_parameterCBs;
        QPushButton* m_remove{};

};
