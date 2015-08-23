#pragma once
#include <QtWidgets>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

class ValueWidget : public QWidget
{
    public:
        using QWidget::QWidget;
        virtual QVariant value() const = 0;
};

template<typename T>
class NumericValueWidget : public ValueWidget
{
    public:
        NumericValueWidget(T value, QWidget* parent = nullptr)
            : ValueWidget{parent}
        {
            auto lay = new MarginLess<QGridLayout>;
            m_valueSBox = new MaxRangeSpinBox<TemplatedSpinBox<T>>(this);
            lay->addWidget(m_valueSBox);
            m_valueSBox->setValue(value);
            this->setLayout(lay);
        }

        QVariant value() const override
        {
            return m_valueSBox->value();
        }

    private:
        typename TemplatedSpinBox<T>::spinbox_type* m_valueSBox;
};


class StringValueWidget : public ValueWidget
{
    public:
        StringValueWidget(const QString& value, QWidget* parent = nullptr)
            : ValueWidget{parent}
        {
            auto lay = new MarginLess<QGridLayout>;
            m_value = new QLineEdit;
            lay->addWidget(m_value);
            m_value->setText(value);
            this->setLayout(lay);
        }

        QVariant value() const override
        {
            return m_value->text();
        }

    private:
        QLineEdit* m_value;
};


class BoolValueWidget : public ValueWidget
{
    public:
        BoolValueWidget(bool value, QWidget* parent = nullptr)
            : ValueWidget{parent}
        {
            auto lay = new MarginLess<QGridLayout>;
            m_value = new QComboBox;
            m_value->addItems({tr("False"), tr("True")});

            lay->addWidget(m_value);
            m_value->setCurrentIndex(value ? 1 : 0);
            this->setLayout(lay);
        }

        QVariant value() const override
        {
            return bool(m_value->currentIndex());
        }

    private:
        QComboBox* m_value;
};
