#include "ValueWrapper.hpp"

#include <State/Widgets/Values/NumericValueWidget.hpp>

#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/ClearLayout.hpp>

ValueWrapper::ValueWrapper(QWidget *parent):
    QWidget{parent}
{
    m_lay = new iscore::MarginLess<QGridLayout>;
    this->setLayout(m_lay);
}

void ValueWrapper::setWidget(ValueWidget *widg)
{
    iscore::clearLayout(m_lay);
    m_widget = widg;

    if(widg)
        m_lay->addWidget(m_widget);
}

ValueWidget *ValueWrapper::valueWidget() const
{
    return m_widget;
}
