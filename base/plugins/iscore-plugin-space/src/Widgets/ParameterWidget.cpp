#include "ParameterWidget.hpp"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

namespace Space
{
ParameterWidget::ParameterWidget()
{
    auto lay = new iscore::MarginLess<QHBoxLayout>;
    this->setLayout(lay);

    m_address = new QLineEdit;
    m_defaultValue = new iscore::SpinBox<float>;
    lay->addWidget(m_address);
    lay->addWidget(m_defaultValue);

}
}
