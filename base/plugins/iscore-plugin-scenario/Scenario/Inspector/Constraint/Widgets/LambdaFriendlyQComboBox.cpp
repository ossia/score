#include <QGridLayout>
#include <QLayout>

#include "LambdaFriendlyQComboBox.hpp"

LambdaFriendlyQComboBox::LambdaFriendlyQComboBox(QWidget* parent):
    QWidget{parent}
{
    connect(&m_combobox, SIGNAL(activated(QString)),
            this,        SIGNAL(activated(QString)));

   setLayout(new QGridLayout);
   layout()->addWidget(&m_combobox);
}
