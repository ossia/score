#include <QComboBox>
#include <QGridLayout>
#include <QLabel>

#include <QRadioButton>
#include <QString>
#include <QLineEdit>
#include <QVariant>
#include <QFormLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include "MIDIProtocolSettingsWidget.hpp"
#include "MIDISpecificSettings.hpp"
class QWidget;

namespace Engine
{
namespace Network
{
MIDIProtocolSettingsWidget::MIDIProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
    m_name = new QLineEdit;
    m_inButton = new QCheckBox(tr("Send"), this);
    m_inButton->setAutoExclusive(true);
    m_outButton = new QCheckBox(tr("Receive"), this);
    m_outButton->setAutoExclusive(true);
    m_deviceCBox = new QComboBox(this);

    auto gb_lay = new QHBoxLayout;
    gb_lay->setContentsMargins(0, 0, 0, 0);
    gb_lay->addWidget(m_inButton);
    gb_lay->addWidget(m_outButton);

    auto lay = new QFormLayout;
    lay->addRow(tr("Name"), m_name);
    lay->addRow(tr("Type"), gb_lay);
    lay->addRow(tr("Device"), m_deviceCBox);

    setLayout(lay);

    this->setTabOrder(m_name, m_inButton);
    this->setTabOrder(m_inButton, m_deviceCBox);

    connect(m_inButton, &QAbstractButton::toggled,
            this, [this] (bool b) {
        if(b)
        {
            updateDevices(ossia::net::midi::midi_info::Type::RemoteInput);
        }
    });
    connect(m_outButton, &QAbstractButton::toggled,
            this, [this] (bool b) {
        if(b)
        {
            updateDevices(ossia::net::midi::midi_info::Type::RemoteOutput);
        }
    });


    m_inButton->setChecked(true);  //TODO: QSettings
    updateInputDevices();
}


Device::DeviceSettings MIDIProtocolSettingsWidget::getSettings() const
{
    ISCORE_ASSERT(m_deviceCBox);
    ISCORE_ASSERT(m_inButton);

    // TODO *** Initialize with ProtocolFactory.defaultSettings().
    Device::DeviceSettings s;
    Network::MIDISpecificSettings midi;
    s.name = m_name->text();

    midi.io = m_inButton->isChecked()
              ? Network::MIDISpecificSettings::IO::In
              : Network::MIDISpecificSettings::IO::Out;
    midi.endpoint = m_deviceCBox->currentText();
    midi.port = m_deviceCBox->currentData().toInt();

    s.deviceSpecificSettings = QVariant::fromValue(midi);

    return s;
}

void
MIDIProtocolSettingsWidget::setSettings(const Device::DeviceSettings &settings)
{
    m_name->setText(settings.name);
    int index = m_deviceCBox->findText(settings.name);

    if(index >= 0 && index < m_deviceCBox->count())
    {
        m_deviceCBox->setCurrentIndex(index);
    }

    if (settings.deviceSpecificSettings.canConvert<Network::MIDISpecificSettings>())
    {
        Network::MIDISpecificSettings midi = settings.deviceSpecificSettings.value<Network::MIDISpecificSettings>();
        if(midi.io == Network::MIDISpecificSettings::IO::In)
        {
            m_inButton->setChecked(true);
        }
        else
        {
            m_outButton->setChecked(true);
        }

        m_deviceCBox->setCurrentText(midi.endpoint);
        // TODO <!> setData <!> (midi.port)
    }
}

void MIDIProtocolSettingsWidget::updateDevices(ossia::net::midi::midi_info::Type t)
{
    try {
    auto prot = std::make_unique<ossia::net::midi::midi_protocol>();
    auto vec = prot->scan();

    m_deviceCBox->clear();
    for(auto& elt : vec)
    {
        if(elt.type == t)
        {
            m_deviceCBox->addItem(QString::fromStdString(elt.device), QVariant::fromValue(elt.port));
        }
    }
    m_deviceCBox->setCurrentIndex(0);
    qDebug() << m_deviceCBox->count();
    }
    catch( std::exception& e)
    {
        qDebug() << e.what();
    }
}

void
MIDIProtocolSettingsWidget::updateInputDevices()
{
    updateDevices(ossia::net::midi::midi_info::Type::RemoteInput);
}

void
MIDIProtocolSettingsWidget::updateOutputDevices()
{
    updateDevices(ossia::net::midi::midi_info::Type::RemoteOutput);
}
}
}
