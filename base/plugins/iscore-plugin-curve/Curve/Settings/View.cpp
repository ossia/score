#include "View.hpp"
#include <QSpinBox>
#include <QCheckBox>
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
    connect(m_sb, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &View::simplificationRatioChanged);

    lay->addRow(tr("Simplification Ratio"), m_sb);

    m_simpl = new QCheckBox;
    connect(m_simpl, &QCheckBox::stateChanged,
            this, [&] (int t) {
        switch(t)
        {
            case Qt::Unchecked:
                simplifyChanged(false);
                break;
            case Qt::Checked:
                simplifyChanged(true);
                break;
            default:
                break;
        }

        });

    lay->addRow(tr("Simplify"), m_simpl);

}

void View::setSimplificationRatio(double val)
{
    if(val != m_sb->value())
        m_sb->setValue(val);
}

void View::setSimplify(bool val)
{
    switch(m_simpl->checkState())
    {
        case Qt::Unchecked:
            if(val)
                m_simpl->setChecked(true);
            break;
        case Qt::Checked:
            if(!val)
                m_simpl->setChecked(false);
            break;
        default:
            break;
    }
}

QWidget *View::getWidget()
{
    return m_widg;
}

}
}
