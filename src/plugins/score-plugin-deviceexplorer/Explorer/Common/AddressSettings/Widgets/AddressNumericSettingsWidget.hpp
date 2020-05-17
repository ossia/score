#pragma once
#include "AddressSettingsWidget.hpp"

#include <State/ValueConversion.hpp>
#include <State/Widgets/UnitWidget.hpp>
#include <State/Widgets/Values/NumericValueWidget.hpp>

#include <score/widgets/SpinBoxes.hpp>

#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/value/value_conversion.hpp>

namespace Explorer
{
template <typename T>
struct DefaultBounds;
template <>
struct DefaultBounds<int>
{
  static const constexpr int min = 0;
  static const constexpr int max = 127;
};
template <>
struct DefaultBounds<float>
{
  static const constexpr float min = 0.;
  static const constexpr float max = 1.;
};
template <typename T>
class AddressNumericSettingsWidget final : public AddressSettingsWidget
{
public:
  explicit AddressNumericSettingsWidget(QWidget* parent = nullptr) : AddressSettingsWidget(parent)
  {
    using namespace score;
    m_valueSBox = new SpinBox<T>(this);
    m_domainEdit = new State::NumericDomainWidget<T>{this};

    m_layout->insertRow(0, makeLabel(tr("Value"), this), m_valueSBox);
    m_layout->insertRow(1, makeLabel(tr("Domain"), this), m_domainEdit);

    connect(m_unit, &State::UnitWidget::unitChanged, this, [=](const State::Unit& u) {
      auto dom = ossia::get_unit_default_domain(u.get());

      if (auto p = dom.v.target<ossia::domain_base<float>>())
      {
        m_domainEdit->set_domain(dom);
      }
    });

    m_valueSBox->setValue(0);
    m_domainEdit->set_domain(ossia::make_domain(DefaultBounds<T>::min, DefaultBounds<T>::max));
  }

  Device::AddressSettings getSettings() const override
  {
    auto settings = getCommonSettings();
    settings.value = T(m_valueSBox->value());
    settings.domain = m_domainEdit->domain();
    return settings;
  }

  Device::AddressSettings getDefaultSettings() const override
  {
    Device::AddressSettings s;
    s.value = T{0};
    s.domain = ossia::make_domain(DefaultBounds<T>::min, DefaultBounds<T>::max);
    return s;
  }

  void setSettings(const Device::AddressSettings& settings) override
  {
    setCommonSettings(settings);
    m_valueSBox->setValue(State::convert::value<T>(settings.value));
    m_domainEdit->set_domain(settings.domain);
  }

  void setCanEditProperties(bool b) override
  {
    AddressSettingsWidget::setCanEditProperties(b);
    m_domainEdit->setEnabled(b);
  }

private:
  typename score::TemplatedSpinBox<T>::spinbox_type* m_valueSBox{};
  State::NumericDomainWidget<T>* m_domainEdit{};
};
}
