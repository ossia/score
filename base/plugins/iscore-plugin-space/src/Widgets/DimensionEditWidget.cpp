#include "DimensionEditWidget.hpp"

#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

DimensionEditWidget::DimensionEditWidget(const DimensionModel& dim, QWidget* parent):
    QWidget{parent},
    m_dim{dim}
{
    auto lay = new MarginLess<QHBoxLayout>;

    m_name = new QLineEdit{m_dim.name()};
    lay->addWidget(m_name);

    m_minBound = new MaxRangeSpinBox<TemplatedSpinBox<float>>;
    m_minBound->setValue(m_dim.sym().domain().min);
    lay->addWidget(m_minBound);

    m_maxBound = new MaxRangeSpinBox<TemplatedSpinBox<float>>;
    m_maxBound->setValue(m_dim.sym().domain().max);
    lay->addWidget(m_maxBound);

    m_remove = new QPushButton{tr("Remove")};
    lay->addWidget(m_remove);

    this->setLayout(lay);
}
