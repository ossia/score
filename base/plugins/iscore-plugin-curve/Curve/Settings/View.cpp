#include "View.hpp"
#include <QSpinBox>
#include <QFormLayout>
namespace Curve
{
namespace Settings
{

View::View():
    m_widg{new QWidget}
{
    auto lay = new QFormLayout;
    m_widg->setLayout(lay);

    m_sb = new QDoubleSpinBox;
    m_sb->setMinimum(1);
    m_sb->setMaximum(100);
    lay->addRow(tr("Simplification Ratio"), m_sb);

    connect(m_sb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &View::simplificationRatioChanged);
}

void View::setSimplificationRatio(double val)
{
    if(val != m_sb->value())
        m_sb->setValue(val);
}

QWidget *View::getWidget()
{
    return m_widg;
}

}
}
