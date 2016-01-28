#pragma once
#include "ValueWidget.hpp"

#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

#include <QGridLayout>
template<typename T>
class ISCORE_LIB_STATE_EXPORT  NumericValueWidget : public ValueWidget
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
