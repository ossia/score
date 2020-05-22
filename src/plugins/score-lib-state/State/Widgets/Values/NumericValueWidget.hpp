#pragma once
#include "ValueWidget.hpp"

#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SpinBoxes.hpp>
#include <score/widgets/TextLabel.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace State
{
template <typename T>
using MatchingSpinbox = typename score::TemplatedSpinBox<T>::spinbox_type;

template <typename T>
class NumericValueWidget final : public ValueWidget
{
public:
  NumericValueWidget(T value, QWidget* parent = nullptr) : ValueWidget{parent}
  {
    auto lay = new score::MarginLess<QGridLayout>{this};
    m_valueSBox = new score::SpinBox<T>{this};
    lay->addWidget(m_valueSBox);
    m_valueSBox->setValue(value);
  }

  ossia::value value() const override { return ossia::value{m_valueSBox->value()}; }

private:
  score::SpinBox<T>* m_valueSBox{};
};

template <typename T>
class NumericValueSetDialog final : public QDialog
{
public:
  using set_type = ossia::flat_set<T>;
  NumericValueSetDialog(QWidget* parent) : QDialog{parent}
  {
    auto lay = new score::MarginLess<QVBoxLayout>{this};
    this->setLayout(lay);
    lay->addLayout(m_lay = new score::MarginLess<QVBoxLayout>);

    auto addbutton = new QPushButton{tr("+"), this};
    connect(addbutton, &QPushButton::pressed, this, [=] { addRow({}); });
    lay->addWidget(addbutton);

    auto buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};

    lay->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  }

  set_type values()
  {
    set_type t;
    for (auto widg : m_widgs)
    {
      t.insert(widg->value().template get<T>());
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
    auto sub_lay = new score::MarginLess<QHBoxLayout>{sub_widg};

    auto minus_b = new QPushButton{tr("-"), this};
    sub_lay->addWidget(minus_b);

    connect(minus_b, &QPushButton::clicked, this, [this, i = m_rows.size()] { removeRow(i); });

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
  using domain_type = ossia::domain_base<T>;
  using set_type = ossia::flat_set<T>;

  NumericDomainWidget(QWidget* parent) : QWidget{parent}
  {
    auto lay = new score::MarginLess<QHBoxLayout>{this};
    this->setLayout(lay);

    m_minCB = new QCheckBox{tr("Min"), this};
    m_maxCB = new QCheckBox{tr("Max"), this};
    m_min = new score::SpinBox<T>{this};
    m_max = new score::SpinBox<T>{this};
    lay->addWidget(m_minCB);
    lay->addWidget(m_min);
    lay->addWidget(m_maxCB);
    lay->addWidget(m_max);

    m_min->setEnabled(false);
    m_max->setEnabled(false);

    connect(m_minCB, &QCheckBox::stateChanged, this, [=](int st) { m_min->setEnabled(bool(st)); });
    connect(m_maxCB, &QCheckBox::stateChanged, this, [=](int st) { m_max->setEnabled(bool(st)); });
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

    if (m_minCB->checkState())
      dom.min = m_min->value();
    else
      dom.min = std::nullopt;

    if (m_maxCB->checkState())
      dom.max = m_max->value();
    else
      dom.max = std::nullopt;

    dom.values = m_values;

    return dom;
  }

  void set_domain(ossia::domain dom_base)
  {
    m_values.clear();
    m_minCB->setCheckState(Qt::Unchecked);
    m_maxCB->setCheckState(Qt::Unchecked);

    if (auto dom_p = dom_base.v.target<domain_type>())
    {
      auto& dom = *dom_p;

      if (dom.min)
      {
        m_minCB->setCheckState(Qt::Checked);
        m_min->setValue(*dom.min);
      }
      if (dom.max)
      {
        m_maxCB->setCheckState(Qt::Checked);
        m_max->setValue(*dom.max);
      }

      m_values = dom.values;
    }
  }

private:
  QCheckBox* m_minCB{};
  QCheckBox* m_maxCB{};
  score::SpinBox<T>* m_min{};
  score::SpinBox<T>* m_max{};

  set_type m_values;
};
}
