#include "ExecutorView.hpp"
#include <QSpinBox>
#include <QFormLayout>
#include <iscore/widgets/SignalUtils.hpp>

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

    m_cb = new QComboBox;
    lay->addRow(tr("Clock source"), m_cb);

    connect(m_sb, SignalUtils::QSpinBox_valueChanged_int(),
            this, &View::rateChanged);

    connect(m_cb, SignalUtils::QComboBox_currentIndexChanged_int(),
            this, [this] (int i) {
        emit clockChanged(m_cb->itemData(i).value<ClockManagerFactory::ConcreteFactoryKey>());
    });
}

void View::setRate(int val)
{
    if(val != m_sb->value())
        m_sb->setValue(val);
}

void View::setClock(ClockManagerFactory::ConcreteFactoryKey k)
{
    int idx = m_cb->findData(QVariant::fromValue(k));
    if(idx != m_cb->currentIndex())
        m_cb->setCurrentIndex(idx);
}

void View::populateClocks(
        const std::map<QString, ClockManagerFactory::ConcreteFactoryKey>& map)
{
    for(auto& elt : map)
    {
        m_cb->addItem(elt.first, QVariant::fromValue(elt.second));
    }
}

QWidget *View::getWidget()
{
    return m_widg;
}

}
}
