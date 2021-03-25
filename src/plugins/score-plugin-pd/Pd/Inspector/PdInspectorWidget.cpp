#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/TextLabel.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Pd/Inspector/PdInspectorWidget.hpp>
#include <score/tools/Bind.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Pd::PdWidget)

namespace Pd
{

PdWidget::PdWidget(
    const Pd::ProcessModel& proc,
    const score::DocumentContext& context,
    QWidget* parent)
    : InspectorWidgetDelegate_T{proc, parent}
    , m_disp{context.commandStack}
    , m_proc{proc}
    , m_explorer{context.plugin<Explorer::DeviceDocumentPlugin>().explorer()}
    , m_lay{}
    , m_portwidg{}
    , m_sublay{&m_portwidg}
    , m_audioIn{&m_portwidg}
    , m_audioOut{&m_portwidg}
    , m_midiIn{&m_portwidg}
    , m_midiOut{&m_portwidg}
{
  setObjectName("PdInspectorWidget");
  setParent(parent);

  this->setLayout(&m_lay);
  m_lay.addWidget(&m_portwidg);
  m_lay.addWidget(&m_ledit);
  m_ledit.setText(proc.script());
  m_lay.addStretch(1);

  m_sublay.addRow("Audio input channels", &m_audioIn);
  m_sublay.addRow("Audio output channels", &m_audioOut);
  m_sublay.addRow("Midi in", &m_midiIn);
  m_sublay.addRow("Midi out", &m_midiOut);

  con(m_ledit, &QLineEdit::editingFinished, this, [&] {
    CommandDispatcher<> cmd{context.commandStack};
    cmd.submit<EditPdPath>(proc, m_ledit.text());
  });

  m_audioIn.setValue(m_proc.audioInputs());
  m_audioOut.setValue(m_proc.audioOutputs());
  m_midiIn.setChecked(m_proc.midiInput());
  m_midiOut.setChecked(m_proc.midiOutput());
  con(m_audioIn, SignalUtils::QSpinBox_valueChanged_int(), this, [&](int val) {
    if (val != m_proc.audioInputs())
      m_disp.submit<SetAudioIns>(m_proc, val);
  });
  con(m_audioOut,
      SignalUtils::QSpinBox_valueChanged_int(),
      this,
      [&](int val) {
        if (val != m_proc.audioOutputs())
          m_disp.submit<SetAudioOuts>(m_proc, val);
      });
  con(m_midiIn, &QCheckBox::toggled, this, [&](bool val) {
    if (val != m_proc.midiInput())
      m_disp.submit<SetMidiIn>(m_proc, val);
  });
  con(m_midiOut, &QCheckBox::toggled, this, [&](bool val) {
    if (val != m_proc.midiOutput())
      m_disp.submit<SetMidiOut>(m_proc, val);
  });

  con(proc, &ProcessModel::audioInputsChanged, this, [&](int i) {
    if (m_audioIn.value() != i)
      m_audioIn.setValue(i);
  });
  con(proc, &ProcessModel::audioOutputsChanged, this, [&](int i) {
    if (m_audioOut.value() != i)
      m_audioOut.setValue(i);
  });
  con(proc, &ProcessModel::midiInputChanged, this, [&](bool i) {
    if (m_midiIn.checkState() != i)
      m_midiIn.setChecked(i);
  });
  con(proc, &ProcessModel::midiOutputChanged, this, [&](bool i) {
    if (m_midiOut.checkState() != i)
      m_midiOut.setChecked(i);
  });

  con(proc, &ProcessModel::scriptChanged, this, &PdWidget::on_patchChange);
  reinit();

  con(proc,
      &Process::ProcessModel::inletsChanged,
      this,
      &PdWidget::reinit,
      Qt::QueuedConnection);
  con(proc,
      &Process::ProcessModel::outletsChanged,
      this,
      &PdWidget::reinit,
      Qt::QueuedConnection);
}

void PdWidget::reinit()
{
  /*
  m_inlets.clear();
  m_outlets.clear();

  m_lay.addWidget(new TextLabel{tr("Inlets"), this});

  std::size_t i{};
  for(auto& port : m_proc.inlets())
  {
    auto widg = new Dataflow::PortWidget{m_explorer, this};
    widg->localName.setText(port->customData());
    widg->accessor.setAddress(port->address());

    m_lay.addWidget(widg);
    m_inlets.push_back(widg);
    con(widg->accessor, &Explorer::AddressAccessorEditWidget::addressChanged,
        this, [=] (const Device::FullAddressAccessorSettings& as) {
      auto cur = m_proc.inlets()[i];
      cur->setAddress(as.address);
      auto cmd = new EditPort{m_proc, std::move(cur), i, true};
      m_disp.submit(cmd);
    }, Qt::QueuedConnection);

    con(widg->localName, &QLineEdit::editingFinished,
        this, [=] {
      auto cur = m_proc.inlets()[i];
      cur.customData = widg->localName.text();
      auto cmd = new EditPort{m_proc, std::move(cur), i, true};
      m_disp.submit(cmd);
    }, Qt::QueuedConnection);

    connect(widg, &PortWidget::removeMe, this, [=] {
      m_disp.submit<RemovePort>(m_proc, i, true);
    }, Qt::QueuedConnection);

    i++;
  }
  m_addInlet = new QPushButton{tr("Add inlet"), this};
  connect(m_addInlet, &QPushButton::clicked, this, [&] {
    m_disp.submit<AddPort>(m_proc, true);
  }, Qt::QueuedConnection);
  m_lay.addWidget(m_addInlet);
  m_lay.addWidget(new TextLabel{tr("Outlets"), this});

  i = 0;
  for(auto& port : m_proc.outlets())
  {
    auto widg = new Dataflow::PortWidget{m_explorer, this};
    widg->localName.setText(port->customData());
    widg->accessor.setAddress(port->address());

    m_lay.addWidget(widg);
    m_outlets.push_back(widg);
    con(widg->accessor, &Explorer::AddressAccessorEditWidget::addressChanged,
        this, [=] (const Device::FullAddressAccessorSettings& as) {
      auto cur = m_proc.outlets()[i];
      cur.address = as.address;
      auto cmd = new EditPort{m_proc, std::move(cur), i, false};
      m_disp.submit(cmd);
    }, Qt::QueuedConnection);

    con(widg->localName, &QLineEdit::editingFinished,
        this, [=] {
      auto cur = m_proc.outlets()[i];
      cur.customData = widg->localName.text();
      auto cmd = new EditPort{m_proc, std::move(cur), i, false};
      m_disp.submit(cmd);
    }, Qt::QueuedConnection);

    connect(widg, &PortWidget::removeMe, this, [=] {
      m_disp.submit<RemovePort>(m_proc, i, false);
    }, Qt::QueuedConnection);
    i++;
  }
  m_addOutlet = new QPushButton{tr("Add outlet"), this};
  connect(m_addOutlet, &QPushButton::clicked, this, [&] {
    m_disp.submit<AddPort>(m_proc, false);
  }, Qt::QueuedConnection);
  m_lay.addWidget(m_addOutlet);*/
}
void PdWidget::on_patchChange(const QString& newText)
{
  m_ledit.setText(newText);
}
}
