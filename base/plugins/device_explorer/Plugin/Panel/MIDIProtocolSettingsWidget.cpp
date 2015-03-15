#include "MIDIProtocolSettingsWidget.hpp"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QRadioButton>

#include "NodeFactory.hpp"


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

    connect(m_inButton, SIGNAL(clicked()), this, SLOT(updateInputDevices()));
    connect(m_outButton, SIGNAL(clicked()), this, SLOT(updateOutputDevices()));


    m_inButton->setChecked(true);  //TODO: QSettings
    updateInputDevices();
}


QList<QString>
MIDIProtocolSettingsWidget::getSettings() const
{
    Q_ASSERT(m_deviceCBox);
    Q_ASSERT(m_inButton);

    QList<QString> list;
    list.append(m_deviceCBox->currentText());   //name first !
    list.append(m_inButton->isChecked() ? "In" : "Out");
    return list;
}

void
MIDIProtocolSettingsWidget::setSettings(const QList<QString>& settings)
{
    Q_ASSERT(settings.size() == 2);

    if(settings.at(1) == "In")
    {
        m_inButton->setChecked(true);
    }
    else
    {
        m_outButton->setChecked(true);
    }

    int index = m_deviceCBox->findText(settings.at(0));

    if(index >= 0 && index < m_deviceCBox->count())
    {
        m_deviceCBox->setCurrentIndex(index);
    }

}

void
MIDIProtocolSettingsWidget::updateInputDevices()
{
    //TODO: get input MIDI devices from Model ???
    m_deviceCBox->clear();
    QList<QString> deviceNames = NodeFactory::instance().getAvailableInputMIDIDevices();
    m_deviceCBox->addItems(deviceNames);
}

void
MIDIProtocolSettingsWidget::updateOutputDevices()
{
    //TODO: get output MIDI devices from Model ???
    m_deviceCBox->clear();
    QList<QString> deviceNames = NodeFactory::instance().getAvailableOutputMIDIDevices();
    m_deviceCBox->addItems(deviceNames);
}
