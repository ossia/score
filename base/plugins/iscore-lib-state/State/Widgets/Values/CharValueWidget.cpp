#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/TextLabel.hpp>

#include "CharValueWidget.hpp"
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <State/Widgets/Values/ValueWidget.hpp>

namespace State
{
CharValueWidget::CharValueWidget(QChar value, QWidget* parent)
    : ValueWidget{parent}
{
  auto lay = new iscore::MarginLess<QGridLayout>{this};
  m_value = new QLineEdit;
  m_value->setMaxLength(1);

  lay->addWidget(m_value);
  m_value->setText(value);
}

void CharValueWidget::setValue(Value v) const
{
  m_value->clear();
  if (auto c_ptr = v.val.impl().target<char>()) // BERK
    m_value->setText(QString(QChar(*c_ptr)));
}

State::Value CharValueWidget::value() const
{
  auto txt = m_value->text();
  return State::Value{txt.length() > 0 ? txt[0].toLatin1() : char{}};
}

CharValueSetDialog::CharValueSetDialog(QWidget* parent) : QDialog{parent}
{
  auto lay = new iscore::MarginLess<QVBoxLayout>{this};
  this->setLayout(lay);
  lay->addLayout(m_lay = new iscore::MarginLess<QVBoxLayout>);

  auto addbutton = new QPushButton{tr("+"), this};
  connect(addbutton, &QPushButton::pressed, this, [=] { addRow('a'); });
  lay->addWidget(addbutton);

  auto buttonBox
      = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};

  lay->addWidget(buttonBox);

  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CharValueSetDialog::set_type CharValueSetDialog::values()
{
  set_type t;
  for (auto widg : m_widgs)
  {
    auto val = widg->value();
    char v = val.val.get<QChar>().toLatin1();
    t.insert(v);
  }
  return t;
}

void CharValueSetDialog::setValues(const CharValueSetDialog::set_type& t)
{
  // OPTIMIZEME by reusing
  for (auto row : m_rows)
    delete row;
  m_rows.clear();
  m_widgs.clear();

  for (char val : t)
  {
    addRow(val);
  }
}

void CharValueSetDialog::addRow(char c)
{
  auto sub_widg = new QWidget{this};
  auto sub_lay = new iscore::MarginLess<QHBoxLayout>{sub_widg};

  auto minus_b = new QPushButton{tr("-"), this};
  sub_lay->addWidget(minus_b);

  connect(minus_b, &QPushButton::clicked, this, [ this, i = m_rows.size() ] {
    removeRow(i);
  });

  auto widg = new CharValueWidget{QChar(c), this};
  sub_lay->addWidget(widg);

  m_lay->addWidget(sub_widg);
  m_rows.push_back(sub_widg);
  m_widgs.push_back(widg);
}

void CharValueSetDialog::removeRow(std::size_t i)
{
  if (i < m_rows.size())
  {
    delete m_rows[i];
    m_rows.erase(m_rows.begin() + i);
    m_widgs.erase(m_widgs.begin() + i);
  }
}

CharDomainWidget::CharDomainWidget(QWidget* parent) : QWidget{parent}
{
  auto lay = new iscore::MarginLess<QHBoxLayout>{this};
  this->setLayout(lay);

  auto min_l = new TextLabel{tr("Min"), this};
  min_l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_min = new QLineEdit{this};
  m_min->setMaximumWidth(20);
  m_min->setMaxLength(1);
  auto max_l = new TextLabel{tr("Max"), this};
  max_l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_max = new QLineEdit{this};
  m_max->setMaximumWidth(20);
  m_max->setMaxLength(1);
  lay->addWidget(min_l);
  lay->addWidget(m_min);
  lay->addWidget(max_l);
  lay->addWidget(m_max);

  auto pb = new QPushButton{tr("Values"), this};
  lay->addWidget(pb);

  connect(pb, &QPushButton::clicked, this, [=] {
    CharValueSetDialog dial{this};
    dial.setValues(m_values);

    if (dial.exec())
    {
      m_values = dial.values();
    }
  });
}

ossia::domain_base<char> CharDomainWidget::domain() const
{
  ossia::domain_base<char> dom;

  auto min = m_min->text();
  if (!min.isEmpty())
    dom.min = min[0].toLatin1();

  auto max = m_max->text();
  if (!max.isEmpty())
    dom.max = max[0].toLatin1();

  dom.values = m_values;

  return dom;
}

void CharDomainWidget::setDomain(ossia::domain dom_base)
{
  m_values.clear();
  m_min->clear();
  m_max->clear();

  if (auto dom_p = dom_base.target<ossia::domain_base<char>>())
  {
    auto& dom = *dom_p;

    if (dom.min)
      m_min->setText(QString(QChar(*dom.min)));
    if (dom.max)
      m_max->setText(QString(QChar(*dom.max)));

    m_values = dom.values;
  }
}
}
