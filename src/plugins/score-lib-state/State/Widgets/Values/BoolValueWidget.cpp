// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BoolValueWidget.hpp"

#include <State/Widgets/Values/ValueWidget.hpp>

#include <score/widgets/MarginLess.hpp>

#include <QComboBox>
#include <QGridLayout>

namespace State
{
BoolValueWidget::BoolValueWidget(bool value, QWidget* parent) : ValueWidget{parent}
{
  auto lay = new score::MarginLess<QGridLayout>{this};
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
