#include <Explorer/Widgets/AddressFragmentLineEdit.hpp>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>

#include <QPushButton>
#include <QSpinBox>
#include <QVariant>

#include "Device/Protocol/ProtocolSettingsWidget.hpp"
#include "OSCProtocolSettingsWidget.hpp"


OSCProtocolSettingsWidget::OSCProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
    buildGUI();
}

void
OSCProtocolSettingsWidget::buildGUI()
{
    QLabel* deviceNameLabel = new QLabel(tr("Device name"), this);
    m_deviceNameEdit = new AddressFragmentLineEdit;

    QLabel* portOutputLabel = new QLabel(tr("Port (destination)"), this);
    m_portOutputSBox = new QSpinBox(this);
    m_portOutputSBox->setRange(0, 65535);

    QLabel* portInputLabel = new QLabel(tr("Port (reception)"), this);
    m_portInputSBox = new QSpinBox(this);
    m_portInputSBox->setRange(0, 65535);

    QLabel* localHostLabel = new QLabel(tr("Host"), this);
    m_localHostEdit = new QLineEdit(this);

    QPushButton* loadNamespaceButton = new QPushButton(tr("Load..."), this);
    loadNamespaceButton->setAutoDefault(false);
    loadNamespaceButton->setToolTip(tr("Load namespace file"));
    m_namespaceFilePathEdit = new QLineEdit(this);


    QGridLayout* gLayout = new QGridLayout;

    gLayout->addWidget(deviceNameLabel, 0, 0, 1, 1);
    gLayout->addWidget(m_deviceNameEdit, 0, 1, 1, 1);

    gLayout->addWidget(portOutputLabel, 1, 0, 1, 1);
    gLayout->addWidget(m_portOutputSBox, 1, 1, 1, 1);
    gLayout->addWidget(portInputLabel, 2, 0, 1, 1);
    gLayout->addWidget(m_portInputSBox, 2, 1, 1, 1);

    gLayout->addWidget(localHostLabel, 3, 0, 1, 1);
    gLayout->addWidget(m_localHostEdit, 3, 1, 1, 1);

    gLayout->addWidget(loadNamespaceButton, 4, 0, 1, 1);
    gLayout->addWidget(m_namespaceFilePathEdit, 4, 1, 1, 1);

    connect(loadNamespaceButton, SIGNAL(clicked()), this, SLOT(openFileDialog()));

    setLayout(gLayout);

    setDefaults();
}

void
OSCProtocolSettingsWidget::setDefaults()
{
    m_deviceNameEdit->setText("OSCdevice");
    m_portOutputSBox->setValue(9997);
    m_portInputSBox->setValue(9996);
    m_localHostEdit->setText("127.0.0.1");
}

#include "OSCSpecificSettings.hpp"

class QWidget;

iscore::DeviceSettings OSCProtocolSettingsWidget::getSettings() const
{
    iscore::DeviceSettings s;
    s.name = m_deviceNameEdit->text();

    OSCSpecificSettings osc;
    osc.host = m_localHostEdit->text();
    osc.inputPort = m_portInputSBox->value();
    osc.outputPort = m_portOutputSBox->value();

    // TODO list.append(m_namespaceFilePathEdit->text());
    s.deviceSpecificSettings = QVariant::fromValue(osc);

    return s;
}

QString OSCProtocolSettingsWidget::getPath() const
{
    return m_namespaceFilePathEdit->text();
}

void
OSCProtocolSettingsWidget::setSettings(const iscore::DeviceSettings &settings)
{
/*
    ISCORE_ASSERT(settings.size() == 5);

    m_namespaceFilePathEdit->setText(settings.at(4));
*/
    m_deviceNameEdit->setText(settings.name);
    OSCSpecificSettings osc;
    if (settings.deviceSpecificSettings.canConvert<OSCSpecificSettings>())
    {
        osc = settings.deviceSpecificSettings.value<OSCSpecificSettings>();
        m_portInputSBox->setValue(osc.inputPort);
        m_portOutputSBox->setValue(osc.outputPort);
        m_localHostEdit->setText(osc.host);
    }

}


void
OSCProtocolSettingsWidget::openFileDialog()
{
    const QString fileName = QFileDialog::getOpenFileName();

    if(! fileName.isEmpty())
    {
        m_namespaceFilePathEdit->setText(fileName);
    }
}
