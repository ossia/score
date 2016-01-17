#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFlags>
#include <QGridLayout>
#include <QLabel>
#include <QLayoutItem>
#include <qnamespace.h>

#include <QVariant>
#include <QWidget>
#include <utility>

#include <Device/Protocol/ProtocolFactoryKey.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include "DeviceEditDialog.hpp"
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

namespace DeviceExplorer
{
DeviceEditDialog::DeviceEditDialog(
        const Device::DynamicProtocolList& pl,
        QWidget* parent)
    : QDialog(parent),
      m_protocolList{pl},
      m_protocolWidget{nullptr},
      m_index(-1)
{
    setModal(true);
    buildGUI();
}

DeviceEditDialog::~DeviceEditDialog()
{

}

void
DeviceEditDialog::buildGUI()
{
    QLabel* protocolLabel = new QLabel(tr("Protocol"), this);
    m_protocolCBox = new QComboBox(this);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
            | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);


    m_gLayout = new QGridLayout;

    // QLabel for the warning text
    m_gLayout->addWidget(new QLabel, 0, 0, 1, 2);

    m_gLayout->addWidget(protocolLabel, 1, 0, 1, 1);
    m_gLayout->addWidget(m_protocolCBox, 1, 1, 1, 1);
    //keep one row for m_protocolWidget
    m_gLayout->addWidget(buttonBox, 3, 0, 1, 2);

    setLayout(m_gLayout);

    initAvailableProtocols(); //populate m_protocolCBox

    connect(m_protocolCBox,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &DeviceEditDialog::updateProtocolWidget);

    if(m_protocolCBox->count() > 0)
    {
        ISCORE_ASSERT(m_protocolCBox->currentIndex() == 0);
        updateProtocolWidget();
    }
}

void
DeviceEditDialog::initAvailableProtocols()
{
    ISCORE_ASSERT(m_protocolCBox);

    //initialize previous settings
    m_previousSettings.clear();

    for(const auto& prot : m_protocolList.list().get())
    {
        m_protocolCBox->addItem(
                    prot.second->prettyName(),
                    QVariant::fromValue(prot.second->key<Device::ProtocolFactoryKey>()));

        m_previousSettings.append(prot.second->defaultSettings());
    }

    m_index = m_protocolCBox->currentIndex();
}



void
DeviceEditDialog::updateProtocolWidget()
{
    ISCORE_ASSERT(m_protocolCBox);

    if(m_protocolWidget)
    {
        ISCORE_ASSERT(m_index < m_protocolCBox->count());
        ISCORE_ASSERT(m_index < m_previousSettings.count());

        m_previousSettings[m_index] = m_protocolWidget->getSettings();
        delete m_protocolWidget;
    }

    m_index = m_protocolCBox->currentIndex();

    auto protocol = m_protocolCBox->currentData().value<Device::ProtocolFactoryKey>();
    m_protocolWidget = m_protocolList.list().get(protocol)->makeSettingsWidget();

    if(m_protocolWidget)
    {
        m_gLayout->addWidget(m_protocolWidget, 2, 0, 1, 2);
        updateGeometry();
    }

}

Device::DeviceSettings DeviceEditDialog::getSettings() const
{
    Device::DeviceSettings settings;

    if(m_protocolWidget)
    {
        settings = m_protocolWidget->getSettings();
    }

    // TODO after set the protocol in getSettings instead.
    settings.protocol = m_protocolCBox->currentData().value<Device::ProtocolFactoryKey>();

    return settings;
}

QString DeviceEditDialog::getPath() const
{
    return m_protocolWidget->getPath();
}

void
DeviceEditDialog::setSettings(const Device::DeviceSettings &settings)
{
    //auto proto = SingletonProtocolList::instance().get(settings.protocol);
    //if(proto)
    {
        const int index = m_protocolCBox->findData(QVariant::fromValue(settings.protocol));
        ISCORE_ASSERT(index != -1);
        ISCORE_ASSERT(index < m_protocolCBox->count());

        m_protocolCBox->setCurrentIndex(index);  //will emit currentIndexChanged(int) & call slot

        m_protocolWidget->setSettings(settings);
    }
    //else
    {
    //    ISCORE_TODO; // Make a default widget.
    }
}

void DeviceEditDialog::setEditingInvalidState(bool st)
{
    if(st != m_invalidState)
    {
        if(st)
        {
            auto item = m_gLayout->itemAtPosition(0, 0);
            static_cast<QLabel*>(item->widget())->setText(tr("Warning : device requires editing."));
        }
        else
        {
            auto item = m_gLayout->takeAt(0);
            delete item->widget();
            delete item;
        }

        m_invalidState = st;
    }
}
}
