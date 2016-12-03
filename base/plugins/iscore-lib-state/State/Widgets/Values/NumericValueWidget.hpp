#pragma once
#include "ValueWidget.hpp"

#include <ossia/network/domain/domain.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace State
{
template <typename T>
using MatchingSpinbox = typename iscore::TemplatedSpinBox<T>::spinbox_type;

template <typename T>
class NumericValueWidget final : public State::ValueWidget
{
public:
  NumericValueWidget(T value, QWidget* parent = nullptr) : ValueWidget{parent}
  {
    auto lay = new iscore::MarginLess<QGridLayout>{this};
    m_valueSBox = new iscore::SpinBox<T>{this};
    lay->addWidget(m_valueSBox);
    m_valueSBox->setValue(value);
  }

  State::Value value() const override
  {
    return State::Value{m_valueSBox->value()};
  }

private:
  iscore::SpinBox<T>* m_valueSBox{};
};

template <typename T>
class NumericValueSetDialog final : public QDialog
{
public:
  using set_type = boost::container::flat_set<T>;
  NumericValueSetDialog(QWidget* parent) : QDialog{parent}
  {
    auto lay = new iscore::MarginLess<QVBoxLayout>{this};
    this->setLayout(lay);
    lay->addLayout(m_lay = new iscore::MarginLess<QVBoxLayout>);

    auto addbutton = new QPushButton{tr("+"), this};
    connect(addbutton, &QPushButton::pressed, this, [=] { addRow({}); });
    lay->addWidget(addbutton);

    auto buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok
                                          | QDialogButtonBox::Cancel};

    lay->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  }

  set_type values()
  {
    set_type t;
    for (auto widg : m_widgs)
    {
      t.insert(widg->value().val.template get<T>());
    }
    return t;
  }

  void setValues(const set_type& t)
  {
    // OPTIMIZEME by reusing
    for (auto row : m_rows)
      delete row;
    m_rows.clear();
    m_widgs.clear();

    for (auto val : t)
    {
      addRow(val);
    }
  }

private:
  void addRow(T c)
  {
    auto sub_widg = new QWidget{this};
    auto sub_lay = new iscore::MarginLess<QHBoxLayout>{sub_widg};

    auto minus_b = new QPushButton{tr("-"), this};
    sub_lay->addWidget(minus_b);

    connect(minus_b, &QPushButton::clicked, this, [ this, i = m_rows.size() ] {
      removeRow(i);
    });

    auto widg = new NumericValueWidget<T>{c, this};
    sub_lay->addWidget(widg);

    m_lay->addWidget(sub_widg);
    m_rows.push_back(sub_widg);
    m_widgs.push_back(widg);
  }

  void removeRow(std::size_t i)
  {
    if (i < m_rows.size())
    {
      delete m_rows[i];
      m_rows.erase(m_rows.begin() + i);
      m_widgs.erase(m_widgs.begin() + i);
    }
  }

  QVBoxLayout* m_lay{};
  std::vector<QWidget*> m_rows;
  std::vector<NumericValueWidget<T>*> m_widgs;
};

template <typename T>
class NumericDomainWidget final : public QWidget
{
public:
  using domain_type = ossia::net::domain_base<T>;
  using set_type = boost::container::flat_set<T>;

  NumericDomainWidget(QWidget* parent) : QWidget{parent}
  {
    auto lay = new iscore::MarginLess<QHBoxLayout>{this};
    this->setLayout(lay);

    auto min_l = new QLabel{tr("Min"), this};
    min_l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_min = new iscore::SpinBox<T>{this};
    auto max_l = new QLabel{tr("Max"), this};
    max_l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_max = new iscore::SpinBox<T>{this};
    lay->addWidget(min_l);
    lay->addWidget(m_min);
    lay->addWidget(max_l);
    lay->addWidget(m_max);

    auto pb = new QPushButton{tr("Values"), this};
    lay->addWidget(pb);

    connect(pb, &QPushButton::clicked, this, [=] {
      NumericValueSetDialog<T> dial{this};
      dial.setValues(m_values);

      if (dial.exec())
      {
        m_values = dial.values();
      }
    });
  }

  domain_type domain() const
  {
    domain_type dom;

    dom.min = m_min->value();
    dom.max = m_max->value();
    dom.values = m_values;

    return dom;
  }

  void setDomain(ossia::net::domain dom_base)
  {
    m_values.clear();
    m_min->setValue(0);
    m_max->setValue(100);

    if (auto dom_p = dom_base.target<domain_type>())
    {
      auto& dom = *dom_p;

      if (dom.min)
        m_min->setValue(*dom.min);
      if (dom.max)
        m_max->setValue(*dom.max);

      m_values = dom.values;
    }
  }

private:
  iscore::SpinBox<T>* m_min{};
  iscore::SpinBox<T>* m_max{};

  set_type m_values;
};
}
