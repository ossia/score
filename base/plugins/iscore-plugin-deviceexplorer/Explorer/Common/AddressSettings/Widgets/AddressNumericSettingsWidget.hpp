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
#include <State/ValueConversion.hpp>
#include <ossia/editor/value/value_conversion.hpp>

namespace Explorer
{
template<typename T>
class AddressNumericSettingsWidget final : public AddressSettingsWidget
{
    public:
        explicit AddressNumericSettingsWidget(QWidget* parent = nullptr)
            : AddressSettingsWidget(parent)
        {
            using namespace iscore;
            m_valueSBox = new SpinBox<T>(this);
            m_minSBox = new SpinBox<T>(this);
            m_maxSBox = new SpinBox<T>(this);

            m_layout->insertRow(0, tr("Value"), m_valueSBox);
            m_layout->insertRow(1, tr("Min"), m_minSBox);
            m_layout->insertRow(2, tr("Max"), m_maxSBox);

            m_valueSBox->setValue(0);
            m_minSBox->setValue(0);
            m_maxSBox->setValue(100);
        }

        Device::AddressSettings getSettings() const override
        {
            auto settings = getCommonSettings();
            settings.value.val = T(m_valueSBox->value());
            settings.domain = ossia::net::make_domain(
                                T(m_minSBox->value()),
                                T(m_maxSBox->value()));
            return settings;
        }

        Device::AddressSettings getDefaultSettings() const override
        {
          Device::AddressSettings s;

          ossia::net::domain_base<T> dom;
          dom.min = 0;
          dom.max = 100;

          s.domain = std::move(dom);
          return s;
        }

        void setSettings(const Device::AddressSettings& settings) override
        {
            setCommonSettings(settings);
            m_valueSBox->setValue(State::convert::value<T>(settings.value));

            m_minSBox->setValue(settings.domain.convert_min<T>());
            m_maxSBox->setValue(settings.domain.convert_max<T>());

            // TODO if the "values" part of the domain is set, we
            // have to display a combobox instead.
        }

    private:
        typename iscore::TemplatedSpinBox<T>::spinbox_type* m_valueSBox;
        typename iscore::TemplatedSpinBox<T>::spinbox_type* m_minSBox;
        typename iscore::TemplatedSpinBox<T>::spinbox_type* m_maxSBox;
};
}

