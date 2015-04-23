#include "DeviceEditDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>

#include <DeviceExplorer/Protocol/ProtocolSettingsWidget.hpp>
#include <Plugin/DeviceExplorerPlugin.hpp>
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>

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

    m_gLayout->addWidget(protocolLabel, 0, 0, 1, 1);
    m_gLayout->addWidget(m_protocolCBox, 0, 1, 1, 1);
    //keep one row for m_protocolWidget
    m_gLayout->addWidget(buttonBox, 2, 0, 1, 2);

    setLayout(m_gLayout);

    initAvailableProtocols(); //populate m_protocolCBox

    connect(m_protocolCBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateProtocolWidget()));

    if(m_protocolCBox->count() > 0)
    {
        Q_ASSERT(m_protocolCBox->currentIndex() == 0);
        updateProtocolWidget();
    }
}

void
DeviceEditDialog::initAvailableProtocols()
{
    Q_ASSERT(m_protocolCBox);

    m_protocolCBox->addItems(SingletonProtocolList::instance().protocols());

    //initialize previous settings
    m_previousSettings.clear();

    for(int i = 0; i < m_protocolCBox->count(); ++i)
    {
        m_previousSettings.append(DeviceSettings());
    }

    m_index = m_protocolCBox->currentIndex();
}



void
DeviceEditDialog::updateProtocolWidget()
{
    Q_ASSERT(m_protocolCBox);

    if(m_protocolWidget)
    {
        Q_ASSERT(m_index < m_protocolCBox->count());
        Q_ASSERT(m_index < m_previousSettings.count());

        m_previousSettings[m_index] = m_protocolWidget->getSettings();
        delete m_protocolWidget;
    }

    m_index = m_protocolCBox->currentIndex();

    const QString protocol = m_protocolCBox->currentText();
    m_protocolWidget = SingletonProtocolList::instance().protocol(protocol)->makeSettingsWidget();

    if(m_protocolWidget)
    {
        m_gLayout->addWidget(m_protocolWidget, 1, 0, 1, 2);
        updateGeometry();
    }

}

DeviceSettings DeviceEditDialog::getSettings() const
{
    DeviceSettings settings;

    if(m_protocolWidget)
    {
        settings = m_protocolWidget->getSettings();
    }

    settings.protocol = m_protocolCBox->currentText();

    return settings;
}

QString DeviceEditDialog::getPath() const
{
    return m_protocolWidget->getPath();
}

void
DeviceEditDialog::setSettings(DeviceSettings &settings)
{

    const QString protocol = settings.protocol;
    const int index = m_protocolCBox->findText(protocol);
    Q_ASSERT(index != -1);
    Q_ASSERT(index < m_protocolCBox->count());

    m_protocolCBox->setCurrentIndex(index);  //will emit currentIndexChanged(int) & call slot

    m_protocolWidget->setSettings(settings);
}
