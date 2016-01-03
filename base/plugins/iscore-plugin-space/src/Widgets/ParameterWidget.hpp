#pragma once
#include <QWidget>
class QLineEdit;
class QDoubleSpinBox;

namespace Space
{
class ParameterWidget : public QWidget
{
    public:
        ParameterWidget();

        auto address() const { return m_address; }
        auto defaultValue() const { return m_defaultValue; }

    private:
        QLineEdit* m_address{};
        QDoubleSpinBox* m_defaultValue{};
};
}
