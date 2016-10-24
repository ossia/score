#pragma once
#include "ValueWidget.hpp"

#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

#include <QGridLayout>

namespace State
{
template<typename T>
class NumericValueWidget : public State::ValueWidget
{
    public:
        NumericValueWidget(
                T value,
                QWidget* parent = nullptr)
            : ValueWidget{parent}
        {
            auto lay = new iscore::MarginLess<QGridLayout>{this};
            m_valueSBox = new iscore::SpinBox<T>(this);
            lay->addWidget(m_valueSBox);
            m_valueSBox->setValue(value);
        }

        State::Value value() const override
        {
            return State::Value{m_valueSBox->value()};
        }

    private:
        typename iscore::TemplatedSpinBox<T>::spinbox_type* m_valueSBox;
};
}
