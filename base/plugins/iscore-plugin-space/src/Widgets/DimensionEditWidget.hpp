#pragma once
#include "src/Space/DimensionModel.hpp"
#include <QtWidgets>

class DimensionEditWidget : public QWidget
{
    public:
        DimensionEditWidget(const DimensionModel& dim, QWidget* parent);

    private:
        const DimensionModel& m_dim;
        QLineEdit* m_name{};
        QDoubleSpinBox* m_minBound{};
        QDoubleSpinBox* m_maxBound{};
        QPushButton* m_remove{};
};
