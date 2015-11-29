#pragma once
#include "State/Value.hpp"
#include "ValueWidget.hpp"

class QComboBox;
class QWidget;

class BoolValueWidget : public ValueWidget
{
    public:
        BoolValueWidget(bool value, QWidget* parent = nullptr);

        iscore::Value value() const override;

    private:
        QComboBox* m_value;
};
