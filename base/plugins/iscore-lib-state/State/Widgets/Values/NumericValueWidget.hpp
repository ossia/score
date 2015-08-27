#pragma once
#include "ValueWidget.hpp"

#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

#include <QGridLayout>
template<typename T>
class NumericValueWidget : public ValueWidget
{
    public:
        NumericValueWidget(
                T value,
                QWidget* parent = nullptr)
            : ValueWidget{parent}
        {
            auto lay = new iscore::MarginLess<QGridLayout>;
            m_valueSBox = new iscore::SpinBox<T>(this);
            lay->addWidget(m_valueSBox);
            m_valueSBox->setValue(value);
            this->setLayout(lay);
        }

        QVariant value() const override
        {
            return m_valueSBox->value();
        }

    private:
        typename iscore::TemplatedSpinBox<T>::spinbox_type* m_valueSBox;
};
