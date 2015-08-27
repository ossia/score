#include "StringValueWidget.hpp"
#include <iscore/widgets/MarginLess.hpp>
#include <QLineEdit>
#include <QGridLayout>

StringValueWidget::StringValueWidget(const QString &value, QWidget *parent)
    : ValueWidget{parent}
{
    auto lay = new iscore::MarginLess<QGridLayout>;
    m_value = new QLineEdit;
    lay->addWidget(m_value);
    m_value->setText(value);
    this->setLayout(lay);
}

QVariant StringValueWidget::value() const
{
    return m_value->text();
}
