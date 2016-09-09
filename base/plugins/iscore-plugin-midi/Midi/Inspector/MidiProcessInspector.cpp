#include "MidiProcessInspector.hpp"
#include <Midi/Commands/SetOutput.hpp>

#include <Engine/Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Engine/Protocols/MIDI/MIDISpecificSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SignalUtils.hpp>

#include <QFormLayout>
#include <QComboBox>

namespace Midi
{

InspectorWidget::InspectorWidget(
        const ProcessModel& model,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    InspectorWidgetDelegate_T {model, parent}
{
    auto vlay = new iscore::MarginLess<QFormLayout>{this};
    auto plug = doc.findPlugin<Explorer::DeviceDocumentPlugin>();

    m_devices = new QComboBox;
    vlay->addRow(tr("Midi out"), m_devices);
    for(auto& device : plug->rootNode())
    {
        Device::DeviceSettings set = device.get<Device::DeviceSettings>();
        if(set.protocol == Engine::Network::MIDIProtocolFactory::static_concreteFactoryKey())
        {
            m_devices->addItem(set.name);
        }
    }

    connect(m_devices, SignalUtils::QComboBox_currentIndexChanged_int(),
            this, [&] (int idx) {
        CommandDispatcher<> d{doc.commandStack};
        d.submitCommand(new SetOutput{model, m_devices->itemText(idx)});
    });

    con(model, &ProcessModel::deviceChanged,
        this, [=] (const QString& dev) {
        if(dev != m_devices->currentText())
        {
            m_devices->setCurrentText(dev);
        }
    });

    m_devices->setCurrentText(model.device());
}
}
