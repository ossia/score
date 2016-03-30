#include "View.hpp"
#include <QComboBox>
#include <QFormLayout>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>

namespace Scenario
{
namespace Settings
{

View::View():
    m_widg{new QWidget}
{
    auto lay = new QFormLayout;
    m_widg->setLayout(lay);

    m_skin = new QComboBox;
    m_skin->addItems({"Default", "IEEE"});
    lay->addRow(tr("Skin"), m_skin);

    connect(m_skin, &QComboBox::currentTextChanged,
            this, &View::skinChanged);

    m_zoomSpinBox = new QSpinBox;
    m_zoomSpinBox->setMinimum(50);
    m_zoomSpinBox->setMaximum(300);

    connect(m_zoomSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &View::zoomChanged);

    m_zoomSpinBox->setSuffix(tr("%"));

    lay->addRow(tr("Graphical Zoom \n (50% -- 300%)"), m_zoomSpinBox);
}

void View::setSkin(const QString& val)
{
    if(val != m_skin->currentText())
    {
        int index = m_skin->findText(val);
        if(index != -1)
        {
            m_skin->setCurrentIndex(index);
        }
    }
}

void View::setZoom(const int val)
{
    if(val != m_zoomSpinBox->value())
        m_zoomSpinBox->setValue(val);
}

QWidget *View::getWidget()
{
    return m_widg;
}

}
}
