#include "View.hpp"
#include <QComboBox>
#include <QFormLayout>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <iscore/widgets/SignalUtils.hpp>

namespace Scenario
{
namespace Settings
{

View::View():
    m_widg{new QWidget}
{
    auto lay = new QFormLayout;
    m_widg->setLayout(lay);

    // SKIN
    m_skin = new QComboBox;
    m_skin->addItems({"Default", "IEEE"});
    lay->addRow(tr("Skin"), m_skin);

    connect(m_skin, &QComboBox::currentTextChanged,
            this, &View::skinChanged);

    // ZOOM
    m_zoomSpinBox = new QSpinBox;
    m_zoomSpinBox->setMinimum(50);
    m_zoomSpinBox->setMaximum(300);

    connect(m_zoomSpinBox, SignalUtils::QSpinBox_valueChanged_int(),
            this, &View::zoomChanged);

    m_zoomSpinBox->setSuffix(tr("%"));

    lay->addRow(tr("Graphical Zoom \n (50% -- 300%)"), m_zoomSpinBox);

    //SLOT HEIGHT
    m_slotHeightBox = new QSpinBox;
    m_slotHeightBox->setMinimum(0);
    m_slotHeightBox->setMaximum(10000);

    connect(m_slotHeightBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &View::slotHeightChanged);

    lay->addRow(tr("Default Slot Height"), m_slotHeightBox);
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

void View::setSlotHeight(const double val)
{
    if(val != m_slotHeightBox->value())
        m_slotHeightBox->setValue(val);
}

QWidget *View::getWidget()
{
    return m_widg;
}

}
}
