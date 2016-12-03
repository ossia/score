#pragma once
#include "AddressSettingsWidget.hpp"
#include <ossia/editor/value/value_conversion.hpp>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <State/ValueConversion.hpp>
#include <State/Widgets/Values/NumericValueWidget.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

namespace Explorer
{
template <typename T>
class AddressNumericSettingsWidget final : public AddressSettingsWidget
{
public:
  explicit AddressNumericSettingsWidget(QWidget* parent = nullptr)
      : AddressSettingsWidget(parent)
  {
    using namespace iscore;
    m_valueSBox = new SpinBox<T>(this);
    m_domainEdit = new State::NumericDomainWidget<T>{this};

    m_layout->insertRow(0, makeLabel(tr("Value"), this), m_valueSBox);
    m_layout->insertRow(1, makeLabel(tr("Domain"), this), m_domainEdit);

    m_valueSBox->setValue(0);
    m_domainEdit->setDomain(ossia::net::make_domain(T{0}, T{100}));
  }

  Device::AddressSettings getSettings() const override
  {
    auto settings = getCommonSettings();
    settings.value.val = T(m_valueSBox->value());
    settings.domain = m_domainEdit->domain();
    return settings;
  }

  Device::AddressSettings getDefaultSettings() const override
  {
    Device::AddressSettings s;
    s.value.val = T{0};
    s.domain = ossia::net::make_domain(T{0}, T{100});
    return s;
  }

  void setSettings(const Device::AddressSettings& settings) override
  {
    setCommonSettings(settings);
    m_valueSBox->setValue(State::convert::value<T>(settings.value));
    m_domainEdit->setDomain(settings.domain);
  }

private:
  typename iscore::TemplatedSpinBox<T>::spinbox_type* m_valueSBox{};
  State::NumericDomainWidget<T>* m_domainEdit{};
};
}
