#include <iscore/widgets/MarginLess.hpp>
#include <QComboBox>
#include <QGridLayout>

#include <QString>

#include "BoolValueWidget.hpp"
#include <State/Widgets/Values/ValueWidget.hpp>

class QWidget;

BoolValueWidget::BoolValueWidget(bool value, QWidget *parent)
    : ValueWidget{parent}
{
    auto lay = new iscore::MarginLess<QGridLayout>;
    m_value = new QComboBox;
    m_value->addItems({tr("false"), tr("true")});

    lay->addWidget(m_value);
    m_value->setCurrentIndex(value ? 1 : 0);
    this->setLayout(lay);
}

State::Value BoolValueWidget::value() const
{
    return State::Value{bool(m_value->currentIndex())};
}
