#include "AudioDevice.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/RoutingMatrix.hpp>
#include <Audio/Settings/Model.hpp>

#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/audio/audio_engine.hpp>
#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <functional>
#include <limits>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Dataflow::AudioDevice)
namespace Dataflow
{
AudioDevice::AudioDevice(const Device::DeviceSettings& settings)
    : DeviceInterface{settings}
    , m_protocol{new ossia::audio_protocol}
{
  m_protocol->defer_port_changes(true);
  m_dev = std::make_shared<ossia::net::generic_device>(
      std::unique_ptr<ossia::net::protocol_base>(m_protocol), "audio");
  m_capas.canAddNode = true;
  m_capas.canRemoveNode = true;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canRefreshTree = true;
  m_capas.canRefreshValue = false;
  m_capas.hasCallbacks = false;
  m_capas.canListen = false;
  m_capas.canSerialize = true;
  m_capas.nodeKinds = Device::NodeKind::AudioIn | Device::NodeKind::AudioOut;
}

AudioDevice::~AudioDevice() { }

namespace
{
// The audio callback walks the protocol's lists of ports: they change
// between two of its ticks, while the audio goes on.
void betweenTicks(const std::function<void()>& f)
{
  auto& engine
      = score::GUIAppContext().guiApplicationPlugin<Audio::ApplicationPlugin>().audio;
  if(engine)
    engine->run_between_ticks(f);
  else
    f();
}

void collectParameters(
    ossia::net::node_base& node, std::vector<ossia::net::parameter_base*>& params)
{
  if(auto p = node.get_parameter())
    params.push_back(p);
  for(auto child : node.children_copy())
    collectParameters(*child, params);
}
}

// The gain the user gave a port, saved with the document.
static void applyGain(ossia::net::node_base& node, const ossia::extended_attributes& attr)
{
  auto it = attr.find("audio-gain");
  if(it == attr.end())
    return;
  auto p = dynamic_cast<ossia::audio_parameter*>(node.get_parameter());
  if(!p)
    return;
  if(auto g = ossia::any_cast<double>(&it->second))
    p->push_value(float(*g));
}

void AudioDevice::publishPorts(const std::function<void()>& change)
{
  if(!m_protocol)
    return;
  // Made here, swapped in there; the previous lists are freed here.
  auto ports = m_protocol->current_ports();
  betweenTicks([&] {
    if(change)
      change();
    m_protocol->swap_ports(ports);
  });
}

void AudioDevice::addAddress(const Device::FullAddressSettings& settings)
{
  using namespace ossia;
  this->m_customAddresses[settings.address.path] = settings;
  if(auto dev = getDevice())
  {
    // Create the node. It is added into the device.
    if(auto node = Device::createNodeFromPath(settings.address.path, *dev))
    {
      publishPorts(setupNode(*node, settings.extendedAttributes));
      // The value saved in the document, e.g. the master volume.
      if(settings.value.valid())
        if(auto p = node->get_parameter())
          p->set_value(settings.value);
      applyGain(*node, settings.extendedAttributes);
    }
  }
  portsChanged();
}

void AudioDevice::updateAddress(
    const State::Address& currentAddr, const Device::FullAddressSettings& settings)
{
  this->m_customAddresses[settings.address.path] = settings;
  if(auto dev = getDevice())
  {
    if(auto node = Device::findNodeFromPath(currentAddr.path, *dev))
    {
      publishPorts(setupNode(*node, settings.extendedAttributes));
      applyGain(*node, settings.extendedAttributes);

      auto newName = settings.address.path.last();
      if(!latin_compare(newName, node->get_name()))
      {
        renameListening_impl(currentAddr, newName);
        node->set_name(newName.toStdString());
      }
    }
  }
  portsChanged();
}

void AudioDevice::setGain(const State::Address& addr, double gain)
{
  auto& saved = m_customAddresses[addr.path];
  saved.address = addr;
  saved.extendedAttributes["audio-gain"] = gain;
  if(auto dev = getDevice())
  {
    if(auto node = Device::findNodeFromPath(addr.path, *dev))
    {
      auto x = node->get_extended_attributes();
      x["audio-gain"] = gain;
      node->set_extended_attributes(x);
      applyGain(*node, x);
    }
  }
}

void AudioDevice::removeNode(const State::Address& currentAddr)
{
  this->m_customAddresses.erase(currentAddr.path);
  if(auto dev = getDevice(); dev && m_protocol)
  {
    if(auto node = Device::findNodeFromPath(currentAddr.path, *dev))
    {
      // The ports under it leave the callback's lists before they are destroyed.
      std::vector<ossia::net::parameter_base*> params;
      collectParameters(*node, params);
      for(auto p : params)
      {
        if(auto m = dynamic_cast<ossia::mapped_audio_parameter*>(p))
          m_protocol->unregister_parameter(*m);
        else if(auto v = dynamic_cast<ossia::virtual_audio_parameter*>(p))
          m_protocol->unregister_parameter(*v);
      }
      publishPorts();
    }
  }
  DeviceInterface::removeNode(currentAddr);
  portsChanged();
}

void AudioDevice::disconnect()
{
  // TODO handle listening ??
  setLogging_impl(Device::get_cur_logging(isLogging()));
  auto d = std::move(m_dev);
  deviceChanged(d.get(), nullptr);
  m_dev.reset();
  m_protocol = nullptr;
  m_builtFor.reset();
}

bool AudioDevice::reconnect()
{
  auto& engine
      = score::GUIAppContext().guiApplicationPlugin<Audio::ApplicationPlugin>().audio;

  // The tree depends on the engine and its channels only: when those did not
  // change, e.g. when the device is shown in the explorer while it plays,
  // there is nothing to rebuild, and rebuilding would stop the audio.
  if(m_dev && m_protocol && engine && engine == m_builtFor.lock()
     && engine->effective_inputs == m_builtInputs
     && engine->effective_outputs == m_builtOutputs)
    return true;

  const auto old_dev = m_dev.get();

  struct announce_on_exit
  {
    AudioDevice& self;
    ossia::net::device_base* old_dev;
    ~announce_on_exit()
    {
      // disconnect() already announced old_dev -> nullptr.
      if(auto new_dev = self.m_dev.get(); new_dev && new_dev != old_dev)
        self.deviceChanged(nullptr, new_dev);
      self.changed();
    }
  } announce{*this, old_dev};

  // Park the audio thread on the default tick and wait for it to get there:
  // it must not be walking the tree we are about to destroy.
  if(engine)
    engine->set_tick({});

  // The tree is rebuilt: carry the current parameter values over to it.
  std::vector<std::pair<std::string, ossia::value>> values;
  if(old_dev)
  {
    ossia::net::iterate_all_children(
        &old_dev->get_root_node(), [&](ossia::net::parameter_base& p) {
      values.emplace_back(p.get_node().osc_address(), p.value());
    });
  }

  disconnect();

  try
  {
    m_protocol = new ossia::audio_protocol;
    m_protocol->defer_port_changes(true);
    m_dev = std::make_shared<ossia::net::generic_device>(
        std::unique_ptr<ossia::net::protocol_base>(m_protocol), "audio");
    if(!engine)
      return false;

    m_protocol->setup_tree(engine->effective_inputs, engine->effective_outputs);
    m_builtFor = engine;
    m_builtInputs = engine->effective_inputs;
    m_builtOutputs = engine->effective_outputs;

    // Recreate the custom addresses that were lost in disconnect()
    for(auto& [k, v] : this->m_customAddresses)
    {
      auto node = Device::findNodeFromPath(k, *m_dev);
      if(node)
      {
        if(auto change = setupNode(*node, v.extendedAttributes))
          change();
        applyGain(*node, v.extendedAttributes);
      }
      else
      {
        if(k.size() == 2 && (k[0] == "in" || k[0] == "out")
           && ossia::all_of(k[1], [](QChar c) { return c.isDigit(); }))
          continue;

        node = Device::createNodeFromPath(k, *m_dev);
        if(node)
        {
          (void)setupNode(*node, v.extendedAttributes);
          applyGain(*node, v.extendedAttributes);
        }
      }

      // The value saved in the document, e.g. the master volume.
      if(node && v.value.valid())
        if(auto p = node->get_parameter())
          p->set_value(v.value);
    }

    for(const auto& [address, value] : values)
      if(auto node = ossia::net::find_node(m_dev->get_root_node(), address))
        if(auto p = node->get_parameter())
          p->set_value(value);

    // Nothing walks the new tree yet: the default tick is running.
    auto ports = m_protocol->current_ports();
    m_protocol->swap_ports(ports);
    setLogging_impl(Device::get_cur_logging(isLogging()));
  }
  catch(std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch(...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

void AudioDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
}

std::function<void()> AudioDevice::setupNode(
    ossia::net::node_base& node, const ossia::extended_attributes& attr)
try
{
  auto kind_it = attr.find("audio-kind");
  if(kind_it == attr.end())
    return {}; // it will be added automatically

  // An existing port is changed in place: running processes may hold a
  // pointer to it. Changing it from mapped to virtual or back takes a
  // reconnection. What the audio callback reads changes between two ticks,
  // in the function returned here; everything else is prepared here.
  std::function<void()> change;
  auto kind = ossia::any_cast<std::string>(kind_it->second);
  auto param = node.get_parameter();
  if(kind == "in" || kind == "out")
  {
    const bool output = kind == "out";
    auto chans = ossia::any_cast<ossia::audio_mapping>(attr.at("audio-mapping"));
    for(auto& c : chans)
      if(c < 0)
        c = std::numeric_limits<int>::max();
    if(!param)
    {
      node.set_parameter(
          std::make_unique<ossia::mapped_audio_parameter>(output, chans, node));
    }
    else if(auto mapped = dynamic_cast<ossia::mapped_audio_parameter*>(param))
    {
      if(mapped->is_output != output && m_protocol)
      {
        m_protocol->unregister_parameter(*mapped);
        (output ? m_protocol->out_mappings : m_protocol->in_mappings).push_back(mapped);
      }
      change = [mapped, output, upstream = m_protocol ? m_protocol->main_audio_in : nullptr,
                mapping = std::make_shared<ossia::audio_mapping>(std::move(chans))] {
        mapped->is_output = output;
        mapped->stage = output ? ossia::audio_parameter::gain_stage::push
                               : ossia::audio_parameter::gain_stage::pull;
        mapped->upstream = output ? nullptr : upstream;
        std::swap(mapped->mapping, *mapping);
      };
    }
  }
  else if(kind == "virtual")
  {
    auto chans = ossia::any_cast<int>(attr.at("audio-channels"));
    if(!param)
    {
      node.set_parameter(std::make_unique<ossia::virtual_audio_parameter>(chans, node));
    }
    else if(auto virt = dynamic_cast<ossia::virtual_audio_parameter*>(param))
    {
      change = [virt, buffers = std::make_shared<ossia::virtual_audio_parameter::channels>(
                          virt->make_channels(chans))] { virt->swap_channels(*buffers); };
    }
  }

  auto x = node.get_extended_attributes();
  for(auto& e : attr)
  {
    x[e.first] = e.second;
  }
  node.set_extended_attributes(x);
  return change;
}
catch(...)
{
  return {};
}

Device::Node AudioDevice::refresh()
{
  return simple_refresh();
}

QString AudioProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Audio");
}

QString AudioProtocolFactory::category() const noexcept
{
  return StandardCategories::audio;
}

QUrl AudioProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/audio-device.html");
}

Device::DeviceEnumerators
AudioProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {};
}

Device::DeviceInterface* AudioProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  auto doc = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(doc)
  {
    auto cur = doc->list().audioDevice();
    if(cur)
    {
      // Showing the device in the explorer hands its own settings back: that
      // must not reconnect it, which would stop the audio while it plays.
      if(!(cur->settings() == settings))
        cur->updateSettings(settings);
      return cur;
    }
    else
    {
      auto dev = new Dataflow::AudioDevice(settings);
      doc->list().setAudioDevice(dev);
      return dev;
    }
  }
  else
    return new Dataflow::AudioDevice(settings);
}

const Device::DeviceSettings& AudioProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "audio";
    return s;
  }();
  return settings;
}

class AudioAddressDialog final : public Device::AddressDialog
{
public:
  AudioAddressDialog(
      Dataflow::AudioDevice& dev, const score::DocumentContext& ctx, QWidget* parent)
      : Device::AddressDialog{parent}
      , m_device{dev}
      , m_layout{this}
      , m_nameEdit{this}
      , m_type{this}
      , m_channels{this}
      , m_help{new QLabel{this}}
      , m_mapping{
            new Audio::RoutingMatrix{Audio::RoutingMatrix::Direction::FromCard, this}}
      , m_buttons{
            QDialogButtonBox::StandardButton::Ok
                | QDialogButtonBox::StandardButton::Cancel,
            this}
  {
    this->setMinimumWidth(500);
    m_layout.addRow(tr("Name"), &m_nameEdit);
    m_layout.addRow(tr("Channels"), &m_channels);
    m_layout.addRow(tr("Type"), &m_type);
    m_help->setWordWrap(true);
    m_layout.addRow(m_help);
    m_mapping->setMinimumHeight(160);
    m_layout.addRow(m_mapping);
    m_layout.addWidget(&m_buttons);

    connect(&m_buttons, &QDialogButtonBox::accepted, this, &AudioAddressDialog::accept);
    connect(&m_buttons, &QDialogButtonBox::rejected, this, &AudioAddressDialog::reject);

    m_type.addItems({tr("In"), tr("Out"), tr("Virtual")});
    m_channels.setRange(1, 127);
    m_channels.setValue(2);
    m_type.setCurrentIndex(0);

    con(m_type, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [this] { updateType(); });
    con(m_channels, SignalUtils::QSpinBox_valueChanged_int(), this,
        [this] { updateType(); });
    con(m_nameEdit, &QLineEdit::textChanged, this, [this] { updateType(); });

    updateType();
  }

  AudioAddressDialog(
      const Device::AddressSettings& addr, Dataflow::AudioDevice& dev,
      const score::DocumentContext& ctx, QWidget* parent)
      : AudioAddressDialog{dev, ctx, parent}
  {
    m_original = addr.extendedAttributes;
    m_nameEdit.setText(addr.name);
    const auto& x = addr.extendedAttributes;
    if(auto it = x.find("audio-channels"); it != x.end())
      if(auto c = ossia::any_cast<int>(&it->second))
        m_channels.setValue(*c);
    if(auto it = x.find("audio-kind"); it != x.end())
      if(auto k = ossia::any_cast<std::string>(&it->second))
        m_type.setCurrentIndex(*k == "out" ? 1 : *k == "virtual" ? 2 : 0);
    if(auto it = x.find("audio-mapping"); it != x.end())
      if(auto m = ossia::any_cast<ossia::audio_mapping>(&it->second))
        m_mapping->setPorts({{portName(), *m}});
    updateType();
  }

  QString portName() const
  {
    auto n = m_nameEdit.text().trimmed();
    return n.isEmpty() ? tr("Port") : n;
  }

  void updateType()
  {
    const int k = m_type.currentIndex();
    if(k == 2)
    {
      m_help->setText(
          tr("A virtual port carries sound between processes, not to the sound "
             "card: set the output of some processes to it, and the input of "
             "others."));
      m_mapping->hide();
      return;
    }
    m_help->setText(Audio::RoutingMatrix::help());
    m_mapping->show();

    auto& set = score::AppContext().settings<Audio::Settings::Model>();
    const int cards = k == 0 ? set.getDefaultIn() : set.getDefaultOut();
    m_mapping->setDirection(
        k == 0 ? Audio::RoutingMatrix::Direction::FromCard
               : Audio::RoutingMatrix::Direction::ToCard);
    m_mapping->setCardChannels(cards);

    ossia::int_vector m;
    if(!m_mapping->ports().empty())
      m = m_mapping->ports().front().mapping;
    const int n = m_channels.value();
    const int old = int(m.size());
    m.resize(n);
    for(int i = old; i < n; i++)
      m[i] = i < cards ? i : Audio::unrouted;
    m_mapping->setPorts({{portName(), std::move(m)}});
  }

  Device::AddressSettings getSettings() const override
  {
    Device::AddressSettings addr;
    addr.name = m_nameEdit.text();
    addr.value = ossia::value{};
    addr.extendedAttributes = m_original;
    switch(m_type.currentIndex())
    {
      case 0:
        addr.extendedAttributes["audio-kind"] = std::string("in");
        break;
      case 1:
        addr.extendedAttributes["audio-kind"] = std::string("out");
        break;
      case 2:
        addr.extendedAttributes["audio-kind"] = std::string("virtual");
        break;
    }
    addr.extendedAttributes["audio-channels"] = m_channels.value();
    if(m_type.currentIndex() != 2 && !m_mapping->ports().empty())
      addr.extendedAttributes["audio-mapping"] = m_mapping->ports().front().mapping;
    else
      addr.extendedAttributes.erase("audio-mapping");
    return addr;
  }

  AudioDevice& m_device;

  QFormLayout m_layout;
  State::AddressFragmentLineEdit m_nameEdit;
  QComboBox m_type;
  QSpinBox m_channels;
  QLabel* m_help{};
  Audio::RoutingMatrix* m_mapping{};
  QDialogButtonBox m_buttons;
  ossia::extended_attributes m_original;
};

Device::AddressDialog* AudioProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return new AudioAddressDialog{
      (AudioDevice&)safe_cast<const AudioDevice&>(dev), ctx, parent};
}

Device::AddressDialog* AudioProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return new AudioAddressDialog{
      set, (AudioDevice&)safe_cast<const AudioDevice&>(dev), ctx, parent};
}

Device::ProtocolSettingsWidget* AudioProtocolFactory::makeSettingsWidget()
{
  return new AudioSettingsWidget;
}

QVariant
AudioProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return {};
}

void AudioProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
}

bool AudioProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}

AudioSettingsWidget::AudioSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setEnabled(false);
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);

  setLayout(layout);

  setDefaults();
}

void AudioSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("audio");
}

Device::DeviceSettings AudioSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = AudioProtocolFactory::static_concreteKey();
  return s;
}

void AudioSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}
}
