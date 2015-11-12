#include "DeviceEditDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/SingletonProtocolList.hpp>
DeviceEditDialog::DeviceEditDialog(QWidget* parent)
    : QDialog(parent),
      m_protocolWidget(nullptr), m_index(-1)
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
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));


    m_gLayout = new QGridLayout;

    // QLabel for the warning text
    m_gLayout->addWidget(new QLabel, 0, 0, 1, 2);

    m_gLayout->addWidget(protocolLabel, 1, 0, 1, 1);
    m_gLayout->addWidget(m_protocolCBox, 1, 1, 1, 1);
    //keep one row for m_protocolWidget
    m_gLayout->addWidget(buttonBox, 3, 0, 1, 2);

    setLayout(m_gLayout);

    initAvailableProtocols(); //populate m_protocolCBox

    connect(m_protocolCBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateProtocolWidget()));

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

    const auto& protocols = SingletonProtocolList::instance().get();
    for(const auto& prot : protocols)
        m_protocolCBox->addItem(prot.second->prettyName(), QVariant::fromValue(prot.second->key<ProtocolFactoryKey>()));

    //initialize previous settings
    m_previousSettings.clear();

    for(int i = 0; i < m_protocolCBox->count(); ++i)
    {
        m_previousSettings.append(iscore::DeviceSettings());
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

    auto protocol = m_protocolCBox->currentData().value<ProtocolFactoryKey>();
    m_protocolWidget = SingletonProtocolList::instance().get(protocol)->makeSettingsWidget();

    if(m_protocolWidget)
    {
        m_gLayout->addWidget(m_protocolWidget, 2, 0, 1, 2);
        updateGeometry();
    }

}

iscore::DeviceSettings DeviceEditDialog::getSettings() const
{
    iscore::DeviceSettings settings;

    if(m_protocolWidget)
    {
        settings = m_protocolWidget->getSettings();
    }

    settings.protocol = m_protocolCBox->currentText().toStdString();

    return settings;
}

QString DeviceEditDialog::getPath() const
{
    return m_protocolWidget->getPath();
}

void
DeviceEditDialog::setSettings(const iscore::DeviceSettings &settings)
{
    const auto& protocol = QString::fromStdString(settings.protocol);
    const int index = m_protocolCBox->findText(protocol);
    ISCORE_ASSERT(index != -1);
    ISCORE_ASSERT(index < m_protocolCBox->count());

    m_protocolCBox->setCurrentIndex(index);  //will emit currentIndexChanged(int) & call slot

    m_protocolWidget->setSettings(settings);
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
