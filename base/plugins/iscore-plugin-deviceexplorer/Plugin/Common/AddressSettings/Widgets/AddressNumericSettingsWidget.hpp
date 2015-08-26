#pragma once
#include "AddressSettingsWidget.hpp"
#include <iscore/widgets/SpinBoxes.hpp>
#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QFormLayout>


template<typename T>
class AddressNumericSettingsWidget : public AddressSettingsWidget
{
    public:
        explicit AddressNumericSettingsWidget(QWidget* parent = nullptr)
            : AddressSettingsWidget(parent)
        {
            m_valueSBox = new MaxRangeSpinBox<TemplatedSpinBox<T>>(this);
            m_minSBox = new MaxRangeSpinBox<TemplatedSpinBox<T>>(this);
            m_maxSBox = new MaxRangeSpinBox<TemplatedSpinBox<T>>(this);

            m_layout->insertRow(0, tr("Value"), m_valueSBox);
            m_layout->insertRow(1, tr("Min"), m_minSBox);
            m_layout->insertRow(2, tr("Max"), m_maxSBox);

            m_valueSBox->setValue(0);
            m_minSBox->setValue(0);
            m_maxSBox->setValue(100);
        }

        virtual iscore::AddressSettings getSettings() const override
        {
            auto settings = getCommonSettings();
            settings.value.val = T(m_valueSBox->value());
            settings.domain.min.val = T(m_minSBox->value());
            settings.domain.max.val = T(m_maxSBox->value());
            return settings;
        }

        virtual void setSettings(const iscore::AddressSettings& settings) override
        {
            setCommonSettings(settings);
            if (settings.value.val.canConvert<T>())
            {
                m_valueSBox->setValue(settings.value.val.value<T>());
            }

            m_minSBox->setValue(settings.domain.min.val.toDouble());
            m_maxSBox->setValue(settings.domain.max.val.toDouble());

            // TODO if the "values" part of the domain is set, we
            // have to display a combobox instead.
        }

    private:
        typename TemplatedSpinBox<T>::spinbox_type* m_valueSBox;
        typename TemplatedSpinBox<T>::spinbox_type* m_minSBox;
        typename TemplatedSpinBox<T>::spinbox_type* m_maxSBox;

        QComboBox* m_unitCBox;
};

