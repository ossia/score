#include <QComboBox>
#include <QGridLayout>
#include <iscore/widgets/MarginLess.hpp>

#include <QString>

#include "BoolValueWidget.hpp"
#include <State/Widgets/Values/ValueWidget.hpp>

namespace State
{
BoolValueWidget::BoolValueWidget(bool value, QWidget* parent)
    : ValueWidget{parent}
{
  auto lay = new iscore::MarginLess<QGridLayout>{this};
  m_value = new QComboBox;
  m_value->addItems({tr("false"), tr("true")});

  lay->addWidget(m_value);
  m_value->setCurrentIndex(value ? 1 : 0);
}

ossia::value BoolValueWidget::value() const
{
  return ossia::value{bool(m_value->currentIndex())};
}
}
