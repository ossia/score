#include <QGridLayout>
#include <QLayout>

#include "LambdaFriendlyQComboBox.hpp"

LambdaFriendlyQComboBox::LambdaFriendlyQComboBox(QWidget* parent):
    QWidget{parent}
{
    connect(&m_combobox,  static_cast<void (QComboBox::*)(const QString&)>(&QComboBox::activated),
            this,        &LambdaFriendlyQComboBox::activated);

   setLayout(new QGridLayout);
   layout()->addWidget(&m_combobox);
}
