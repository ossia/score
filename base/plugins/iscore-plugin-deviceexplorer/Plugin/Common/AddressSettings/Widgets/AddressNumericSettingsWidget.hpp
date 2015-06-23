#pragma once
#include "AddressSettingsWidget.hpp"

#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QFormLayout>

template<typename T> struct TemplatedSpinBox;
template<> struct TemplatedSpinBox<int> { using type = QSpinBox; };
template<> struct TemplatedSpinBox<char> { using type = QSpinBox; };
template<> struct TemplatedSpinBox<float> { using type = QDoubleSpinBox; };

template<typename T>
class AddressNumericSettingsWidget : public AddressSettingsWidget
{
    public:
        AddressNumericSettingsWidget(QWidget* parent = nullptr)
            : AddressSettingsWidget(parent)
        {
            m_valueSBox = new typename TemplatedSpinBox<T>::type(this);
            m_minSBox = new typename TemplatedSpinBox<T>::type(this);
            m_maxSBox = new typename TemplatedSpinBox<T>::type(this);

            m_layout->insertRow(0, tr("Value"), m_valueSBox);
            m_layout->insertRow(1, tr("Min"), m_minSBox);
            m_layout->insertRow(2, tr("Max"), m_maxSBox);

            m_valueSBox->setValue(0);
            m_valueSBox->setMinimum(std::numeric_limits<T>::min());
            m_valueSBox->setMaximum(std::numeric_limits<T>::max());

            m_minSBox->setMinimum(std::numeric_limits<T>::min());
            m_minSBox->setMaximum(std::numeric_limits<T>::max());
            m_minSBox->setValue(0);

            m_maxSBox->setMinimum(std::numeric_limits<T>::min());
            m_maxSBox->setMaximum(std::numeric_limits<T>::max());
            m_maxSBox->setValue(100);
        }

        virtual AddressSettings getSettings() const override
        {
            auto settings = getCommonSettings();
            settings.value = T(m_valueSBox->value());
            settings.domain.min = T(m_minSBox->value());
            settings.domain.max = T(m_maxSBox->value());
            return settings;
        }

        virtual void setSettings(const AddressSettings& settings) override
        {
            if (settings.value.canConvert<T>())
            {
                m_valueSBox->setValue(settings.value.value<T>());
            }

            m_minSBox->setValue(settings.domain.min.toDouble());
            m_maxSBox->setValue(settings.domain.max.toDouble());

            // TODO if the "values" part of the domain is set, we
            // have to display a combobox instead.
        }

    private:
        typename TemplatedSpinBox<T>::type* m_valueSBox;
        typename TemplatedSpinBox<T>::type* m_minSBox;
        typename TemplatedSpinBox<T>::type* m_maxSBox;

        QComboBox* m_unitCBox;
};

