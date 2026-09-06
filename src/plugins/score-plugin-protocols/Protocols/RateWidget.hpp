#pragma once
#include <score/widgets/MarginLess.hpp>

#include <ossia/detail/optional.hpp>
#include <ossia/network/rate_limiter_configuration.hpp>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QSpinBox>

#include <verdigris>

namespace Protocols
{
class RateWidget final : public QWidget
{
  W_OBJECT(RateWidget)

public:
  RateWidget(QWidget* parent = nullptr) noexcept
      : QWidget{parent}
      , m_check{new QCheckBox{this}}
      , m_spin{new QSpinBox{this}}
      , m_repeat{new QCheckBox{tr("Repeat"), this}}
      , m_send_all{new QCheckBox{tr("Send all"), this}}
  {
    auto lay = new score::MarginLess<QHBoxLayout>;

    lay->addWidget(m_check);
    lay->addWidget(m_spin);
    lay->addWidget(m_repeat);
    lay->addWidget(m_send_all);
    m_spin->setSizePolicy(QSizePolicy::MinimumExpanding, {});
    m_spin->setSuffix("ms");
    m_spin->setRange(1, 5000);
    lay->setStretch(0, 1);
    lay->setStretch(1, 20);

    connect(m_check, &QCheckBox::toggled, this, [this](bool t) {
      rateChanged(std::optional<int>{});
      m_spin->setEnabled(t);
    });

    m_check->setChecked(false);
    m_spin->setEnabled(false);

    setLayout(lay);
  }

  std::optional<ossia::net::rate_limiter_configuration> configuration() const noexcept
  {
    if(!m_check->isChecked())
    {
      return std::optional<ossia::net::rate_limiter_configuration>{};
    }

    ossia::net::rate_limiter_configuration res;
    res.duration = std::chrono::milliseconds{m_spin->value()};
    res.repeat = m_repeat->isChecked();
    res.send_all = m_send_all->isChecked();
    return res;
  }

  void setConfiguration(std::optional<ossia::net::rate_limiter_configuration> r) noexcept
  {
    if(r)
    {
      m_check->setChecked(true);
      m_spin->setValue(r->duration.count());
      m_repeat->setChecked(r->repeat);
      m_send_all->setChecked(r->send_all);
    }
    else
    {
      m_check->setChecked(false);
    }
  }

  std::optional<int> rate() const noexcept
  {
    if(!m_check->isChecked())
    {
      return std::optional<int>{};
    }
    else
    {
      return m_spin->value();
    }
  }

  void setRate(std::optional<int> r) noexcept
  {
    if(r)
    {
      m_check->setChecked(true);
      m_spin->setValue(*r);
    }
    else
    {
      m_check->setChecked(false);
    }
  }

  void rateChanged(std::optional<int> v) W_SIGNAL(rateChanged, v);

private:
  QCheckBox* m_check{};
  QSpinBox* m_spin{};
  QCheckBox* m_repeat{};
  QCheckBox* m_send_all{};
};

}

W_REGISTER_ARGTYPE(std::optional<int>)
