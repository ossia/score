#pragma once
#include "ValueWidget.hpp"

class QComboBox;
class BoolValueWidget : public ValueWidget
{
    public:
        BoolValueWidget(bool value, QWidget* parent = nullptr);

        QVariant value() const override;

    private:
        QComboBox* m_value;
};
