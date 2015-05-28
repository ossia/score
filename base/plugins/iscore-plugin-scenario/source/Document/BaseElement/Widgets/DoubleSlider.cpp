#include "DoubleSlider.hpp"
#include <QSlider>
#include <numeric>
#include <QGridLayout>

static const constexpr double max = std::numeric_limits<int>::max();

DoubleSlider::DoubleSlider(QWidget* parent):
    QWidget{parent},
    m_slider{new QSlider{Qt::Horizontal}}
{
    m_slider->setContentsMargins(0, 0, 0, 0);
    m_slider->setMinimum(0);
    m_slider->setMaximum(std::numeric_limits<int>::max());

    auto lay = new QGridLayout;
    setLayout(lay);
    lay->addWidget(m_slider);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setMargin(0);
    lay->setSpacing(0);

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
