#include <QComboBox>
#include <QGridLayout>
#include <QLabel>

#include <QRadioButton>
#include <QString>
#include <QVariant>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include "MIDIProtocolSettingsWidget.hpp"
#include "MIDISpecificSettings.hpp"

class QWidget;

MIDIProtocolSettingsWidget::MIDIProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
    buildGUI();
}

void
MIDIProtocolSettingsWidget::buildGUI()
{
    QLabel* ioTypeLabel = new QLabel(tr("Device I/O type"), this);
    m_inButton = new QRadioButton(tr("Input"), this);
    m_outButton = new QRadioButton(tr("Output"), this);
    //radioButtons with same parent are auto-exclusive per default, i.e., behave as belonging to the same group.

    QLabel* midiDeviceLabel = new QLabel(tr("MIDI device"), this);
    m_deviceCBox = new QComboBox(this);

    QGridLayout* gLayout = new QGridLayout;
    gLayout->addWidget(ioTypeLabel, 0, 0, 1, 1);
    gLayout->addWidget(m_inButton, 0, 1, 1, 1);
    gLayout->addWidget(m_outButton, 0, 2, 1, 1);

    gLayout->addWidget(midiDeviceLabel, 1, 0, 1, 1);
    gLayout->addWidget(m_deviceCBox, 1, 1, 1, 2);

    setLayout(gLayout);

    connect(m_inButton, &QAbstractButton::clicked, this, &MIDIProtocolSettingsWidget::updateInputDevices);
    connect(m_outButton, &QAbstractButton::clicked, this, &MIDIProtocolSettingsWidget::updateOutputDevices);


    m_inButton->setChecked(true);  //TODO: QSettings
    updateInputDevices();
}

iscore::DeviceSettings MIDIProtocolSettingsWidget::getSettings() const
{
    ISCORE_ASSERT(m_deviceCBox);
    ISCORE_ASSERT(m_inButton);

    // TODO *** Initialize with ProtocolFactory.defaultSettings().
    iscore::DeviceSettings s;
    MIDISpecificSettings midi;
    s.name = m_deviceCBox->currentText();

    midi.io = m_inButton->isChecked()
              ? MIDISpecificSettings::IO::In
              : MIDISpecificSettings::IO::Out;
    s.deviceSpecificSettings = QVariant::fromValue(midi);

    return s;
}

void
MIDIProtocolSettingsWidget::setSettings(const iscore::DeviceSettings &settings)
{
    /*
    ISCORE_ASSERT(settings.size() == 2);

    if(settings.at(1) == "In")
    {
        m_inButton->setChecked(true);
    }
    else
    {
        m_outButton->setChecked(true);
    }
*/
    int index = m_deviceCBox->findText(settings.name);

    if(index >= 0 && index < m_deviceCBox->count())
    {
        m_deviceCBox->setCurrentIndex(index);
    }

    if (settings.deviceSpecificSettings.canConvert<MIDISpecificSettings>())
    {
        MIDISpecificSettings midi = settings.deviceSpecificSettings.value<MIDISpecificSettings>();
        if(midi.io == MIDISpecificSettings::IO::In)
        {
            m_inButton->setChecked(true);
        }
        else
        {
            m_outButton->setChecked(true);
        }
    }
}

void
MIDIProtocolSettingsWidget::updateInputDevices()
{
    m_deviceCBox->clear();
}

void
MIDIProtocolSettingsWidget::updateOutputDevices()
{
    m_deviceCBox->clear();
}
