// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StringValueWidget.hpp"

#include <State/Widgets/Values/ValueWidget.hpp>

#include <score/widgets/MarginLess.hpp>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

class QWidget;

namespace State
{
StringValueWidget::StringValueWidget(const std::string& value, QWidget* parent)
    : ValueWidget{parent}
{
  auto lay = new score::MarginLess<QGridLayout>{this};
  m_value = new QLineEdit;
  lay->addWidget(m_value);
  m_value->setText(QString::fromStdString(value));
}

ossia::value StringValueWidget::value() const
{
  return ossia::value{m_value->text().toStdString()};
}

StringValueSetDialog::StringValueSetDialog(QWidget* parent) : QDialog{parent}
{
  auto lay = new score::MarginLess<QVBoxLayout>{this};
  this->setLayout(lay);
  lay->addLayout(m_lay = new score::MarginLess<QVBoxLayout>);

  auto addbutton = new QPushButton{tr("+"), this};
  connect(addbutton, &QPushButton::pressed, this, [=] { addRow(""); });
  lay->addWidget(addbutton);

  auto buttonBox = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};

  lay->addWidget(buttonBox);

  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

StringValueSetDialog::set_type StringValueSetDialog::values()
{
  set_type t;
  for (StringValueWidget* widg : m_widgs)
  {
    t.insert(widg->value().v.get<std::string>());
  }
  return t;
}

void StringValueSetDialog::setValues(const StringValueSetDialog::set_type& t)
{
  // OPTIMIZEME by reusing
  for (auto row : m_rows)
    delete row;
  m_rows.clear();
  m_widgs.clear();

  for (auto& val : t)
  {
    addRow(val);
  }
}

void StringValueSetDialog::addRow(const std::string& c)
{
  auto sub_widg = new QWidget{this};
  auto sub_lay = new score::MarginLess<QHBoxLayout>{sub_widg};

  auto minus_b = new QPushButton{tr("-"), this};
  sub_lay->addWidget(minus_b);

  connect(minus_b, &QPushButton::clicked, this, [this, i = m_rows.size()] { removeRow(i); });

  auto widg = new StringValueWidget{c, this};
  sub_lay->addWidget(widg);

  m_lay->addWidget(sub_widg);
  m_rows.push_back(sub_widg);
  m_widgs.push_back(widg);
}

void StringValueSetDialog::removeRow(std::size_t i)
{
  if (i < m_rows.size())
  {
    delete m_rows[i];
    m_rows.erase(m_rows.begin() + i);
    m_widgs.erase(m_widgs.begin() + i);
  }
}
}
