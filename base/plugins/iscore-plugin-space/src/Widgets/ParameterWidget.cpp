#include "ParameterWidget.hpp"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <iscore/widgets/MarginLess.hpp>

ParameterWidget::ParameterWidget()
{
    auto lay = new MarginLess<QHBoxLayout>;
    this->setLayout(lay);

    m_address = new QLineEdit;
    m_defaultValue = new QDoubleSpinBox;
    m_defaultValue->setMinimum(std::numeric_limits<float>::lowest());
    m_defaultValue->setMaximum(std::numeric_limits<float>::max());
    lay->addWidget(m_address);
    lay->addWidget(m_defaultValue);

}
