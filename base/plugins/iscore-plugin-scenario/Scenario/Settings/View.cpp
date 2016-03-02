#include "View.hpp"
#include <QComboBox>
#include <QFormLayout>
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

QWidget *View::getWidget()
{
    return m_widg;
}

}
}
