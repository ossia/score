#include <iscore/widgets/MarginLess.hpp>
#include <QGridLayout>
#include <QLineEdit>
#include <QString>

#include "CharValueWidget.hpp"
#include <State/Widgets/Values/ValueWidget.hpp>

class QWidget;

CharValueWidget::CharValueWidget(QChar value, QWidget *parent)
    : ValueWidget{parent}
{
    auto lay = new iscore::MarginLess<QGridLayout>{this};
    m_value = new QLineEdit;
    m_value->setMaxLength(1);

    lay->addWidget(m_value);
    m_value->setText(value);
}

State::Value CharValueWidget::value() const
{
    auto txt = m_value->text();
    return State::Value{txt.length() > 0 ? txt[0] : QChar{}};
}
