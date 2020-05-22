#pragma once
#include <score/widgets/MarginLess.hpp>

#include <ossia/detail/optional.hpp>

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
      : QWidget{parent}, m_check{new QCheckBox{this}}, m_spin{new QSpinBox{this}}
  {
    auto lay = new score::MarginLess<QHBoxLayout>;

    lay->addWidget(m_check);
    lay->addWidget(m_spin);
    m_spin->setSizePolicy(QSizePolicy::MinimumExpanding, {});
    m_spin->setSuffix("ms");
    m_spin->setRange(1, 5000);
    lay->setStretch(0, 1);
    lay->setStretch(1, 20);

    connect(m_check, &QCheckBox::toggled, this, [=](bool t) {
      rateChanged(std::optional<int>{});
      m_spin->setEnabled(t);
    });

    m_check->setChecked(false);
    m_spin->setEnabled(false);

    setLayout(lay);
  }

  std::optional<int> rate() const noexcept
  {
    if (!m_check->isChecked())
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
    if (r)
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
};

}

W_REGISTER_ARGTYPE(std::optional<int>)
Q_DECLARE_METATYPE(std::optional<int>)
