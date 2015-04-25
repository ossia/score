#include "DoubleSlider.hpp"
#include <QSlider>
#include <numeric>
#include <QGridLayout>
#include <QDebug>
static const constexpr double max = std::numeric_limits<int>::max();

DoubleSlider::DoubleSlider(QWidget* parent):
    QWidget{parent},
    m_slider{new QSlider{Qt::Horizontal}}
{
    m_slider->setMinimum(0);
    m_slider->setMaximum(std::numeric_limits<int>::max());

    setLayout(new QGridLayout);
    layout()->addWidget(m_slider);

    connect(m_slider, &QSlider::valueChanged,
            this, [&] (int val)
    { emit valueChanged(double(val) / max); } );
}

void DoubleSlider::setValue(double val)
{
    m_slider->blockSignals(true);
    m_slider->setValue(val * max);
    m_slider->blockSignals(false);
}

double DoubleSlider::value() const
{
    return m_slider->value() / max;
}
