#include <iscore/widgets/MarginLess.hpp>
#include <QGridLayout>
#include <qnamespace.h>
#include <QSlider>
#include <limits>

#include "DoubleSlider.hpp"

static const constexpr double max = std::numeric_limits<int>::max() / 16384;

DoubleSlider::DoubleSlider(QWidget* parent):
    QWidget{parent},
    m_slider{new QSlider{Qt::Horizontal}}
{
    //m_slider->setContentsMargins(0, 0, 0, 0);
    m_slider->setMinimum(0);
    m_slider->setMaximum(std::numeric_limits<int>::max() / 16384);

    auto lay = new iscore::MarginLess<QGridLayout>;
    lay->addWidget(m_slider);
    setLayout(lay);

    connect(m_slider, &QSlider::valueChanged,
            this, [&] (int val)
    { emit valueChanged(double(val) / max); } );
}

void DoubleSlider::setValue(double val)
{
    if(val>1)
        val = 1;
    m_slider->blockSignals(true);
    m_slider->setValue(val * max);
    m_slider->blockSignals(false);
}

double DoubleSlider::value() const
{
    return m_slider->value() / max;
}
