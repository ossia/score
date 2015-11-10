#include "MinuitProtocolSettingsWidget.hpp"
#include "MinuitSpecificSettings.hpp"
#include <Explorer/Widgets/AddressFragmentLineEdit.hpp>

#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>

MinuitProtocolSettingsWidget::MinuitProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
    buildGUI();
}

void
MinuitProtocolSettingsWidget::buildGUI()
{
    QLabel* deviceNameLabel = new QLabel(tr("Device name"), this);
    m_deviceNameEdit = new AddressFragmentLineEdit;

    QLabel* portInputLabel = new QLabel(tr("Port (input)"), this);
    m_portInputSBox = new QSpinBox(this);
    m_portInputSBox->setRange(0, 65535);

    QLabel* portOutputLabel = new QLabel(tr("Port (output)"), this);
    m_portOutputSBox = new QSpinBox(this);
    m_portOutputSBox->setRange(0, 65535);

    QLabel* localHostLabel = new QLabel(tr("Host"), this);
    m_localHostEdit = new QLineEdit(this);


    QGridLayout* gLayout = new QGridLayout;

    gLayout->addWidget(deviceNameLabel, 0, 0, 1, 1);
    gLayout->addWidget(m_deviceNameEdit, 0, 1, 1, 1);

    gLayout->addWidget(portInputLabel, 1, 0, 1, 1);
    gLayout->addWidget(m_portInputSBox, 1, 1, 1, 1);

    gLayout->addWidget(portOutputLabel, 1, 2, 1, 1);
    gLayout->addWidget(m_portOutputSBox, 1, 3, 1, 1);

    gLayout->addWidget(localHostLabel, 2, 0, 1, 1);
    gLayout->addWidget(m_localHostEdit, 2, 1, 1, 1);

    setLayout(gLayout);

    setDefaults();
}

void
MinuitProtocolSettingsWidget::setDefaults()
{
    ISCORE_ASSERT(m_deviceNameEdit);
    ISCORE_ASSERT(m_portOutputSBox);
    ISCORE_ASSERT(m_localHostEdit);

    m_deviceNameEdit->setText("newDevice");
    m_portInputSBox->setValue(9998);
    m_portOutputSBox->setValue(13579);
    m_localHostEdit->setText("127.0.0.1");
}

iscore::DeviceSettings MinuitProtocolSettingsWidget::getSettings() const
{
    ISCORE_ASSERT(m_deviceNameEdit);

    iscore::DeviceSettings s;
    s.name = m_deviceNameEdit->text();

    MinuitSpecificSettings minuit;
    minuit.host = m_localHostEdit->text();
    minuit.inputPort = m_portInputSBox->value();
    minuit.outputPort = m_portOutputSBox->value();

    s.deviceSpecificSettings = QVariant::fromValue(minuit);
    return s;
}

void
MinuitProtocolSettingsWidget::setSettings(const iscore::DeviceSettings &settings)
{
    m_deviceNameEdit->setText(settings.name);
    MinuitSpecificSettings minuit;
    if(settings.deviceSpecificSettings.canConvert<MinuitSpecificSettings>())
    {
        minuit = settings.deviceSpecificSettings.value<MinuitSpecificSettings>();
        m_portInputSBox->setValue(minuit.inputPort);
        m_portOutputSBox->setValue(minuit.outputPort);
        m_localHostEdit->setText(minuit.host);
    }
}
