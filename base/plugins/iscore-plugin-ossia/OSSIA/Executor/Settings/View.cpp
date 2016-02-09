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

    auto sb = new QSpinBox;
    lay->addRow(tr("Rate"), sb);

    connect(sb, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &View::rateChanged);
}

QWidget *View::getWidget()
{
    return m_widg;
}

}
}
