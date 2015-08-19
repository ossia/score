#include "ParameterWidget.hpp"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

ParameterWidget::ParameterWidget()
{
    auto lay = new MarginLess<QHBoxLayout>;
    this->setLayout(lay);

    m_address = new QLineEdit;
    m_defaultValue = new MaxRangeSpinBox<TemplatedSpinBox<float>>;
    lay->addWidget(m_address);
    lay->addWidget(m_defaultValue);

}
