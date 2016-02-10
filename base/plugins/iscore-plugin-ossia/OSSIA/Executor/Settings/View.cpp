#include "View.hpp"
#include <QSpinBox>
#include <QFormLayout>
namespace RecreateOnPlay
{
namespace Settings
{

View::View():
    m_widg{new QWidget}
{
    auto lay = new QFormLayout;
    m_widg->setLayout(lay);

    m_sb = new QSpinBox;
    m_sb->setMinimum(1);
    m_sb->setMaximum(1000);
    lay->addRow(tr("Granularity"), m_sb);

    connect(m_sb, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &View::rateChanged);
}

void View::setRate(int val)
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
