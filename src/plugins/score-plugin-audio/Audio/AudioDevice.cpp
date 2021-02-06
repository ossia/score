#include "AudioDevice.hpp"
#include <Explorer/DeviceList.hpp>
#include <Explorer/DeviceLogging.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia-qt/js_utilities.hpp>
#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/audio/audio_engine.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>

#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QRadioButton>
#include <QTableWidget>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/Settings/Model.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Dataflow::AudioDevice)
namespace Dataflow
{
AudioDevice::AudioDevice(const Device::DeviceSettings& settings)
    : DeviceInterface{settings}
    , m_protocol{new ossia::audio_protocol}
    , m_dev{std::unique_ptr<ossia::net::protocol_base>(m_protocol), "audio"}
{
  m_capas.canAddNode = true;
  m_capas.canRemoveNode = true;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canRefreshTree = true;
  m_capas.canRefreshValue = false;
  m_capas.hasCallbacks = false;
  m_capas.canListen = false;
  m_capas.canSerialize = true;
}

AudioDevice::~AudioDevice() { }

void AudioDevice::addAddress(const Device::FullAddressSettings& settings)
{
  using namespace ossia;
  if (auto dev = getDevice())
  {
    // Create the node. It is added into the device.
    ossia::net::node_base* node = Device::createNodeFromPath(settings.address.path, *dev);
    SCORE_ASSERT(node);
    setupNode(*node, settings.extendedAttributes);
  }
}

void AudioDevice::updateAddress(
    const State::Address& currentAddr,
    const Device::FullAddressSettings& settings)
{
  if (auto dev = getDevice())
  {
    if (auto node = Device::getNodeFromPath(currentAddr.path, *dev))
    {
      setupNode(*node, settings.extendedAttributes);

      auto newName = settings.address.path.last();
      if (!latin_compare(newName, node->get_name()))
      {
        renameListening_impl(currentAddr, newName);
        node->set_name(newName.toStdString());
      }
    }
  }
}

void AudioDevice::disconnect()
{
  // TODO handle listening ??
  setLogging_impl(Device::get_cur_logging(isLogging()));
}

bool AudioDevice::reconnect()
{
  disconnect();

  try
  {
    auto& proto = static_cast<ossia::audio_protocol&>(m_dev.get_protocol());

    auto& engine = score::GUIAppContext().guiApplicationPlugin<Audio::ApplicationPlugin>().audio;
    if (!engine)
      return false;

    // We have to sync the GUI tree with the audio thread so we stop it momentarily...
    engine->stop();
    proto.setup_tree(engine->effective_inputs, engine->effective_outputs);
    engine->start();

    setLogging_impl(Device::get_cur_logging(isLogging()));
  }
  catch (std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch (...)
  {
    // TODO save the reason of the non-connection.
  }

  qDebug() << "Audio device emits changed:" << this;
  changed();
  return connected();
}

void AudioDevice::recreate(const Device::Node& n)
{
  for (auto& child : n)
  {
    addNode(child);
  }
}

void AudioDevice::setupNode(ossia::net::node_base& node, const ossia::extended_attributes& attr)
{
  // TODO make sure that this function which modifies the tree
  // is done in the execution thread ...
  auto kind_it = attr.find("audio-kind");
  if (kind_it == attr.end())
    return; // it will be added automatically

  auto kind = ossia::any_cast<std::string>(kind_it.value());
  if (kind == "in")
  {
    auto chans = ossia::any_cast<ossia::audio_mapping>(attr.at("audio-mapping"));
    if (!node.get_parameter())
      node.set_parameter(std::make_unique<ossia::mapped_audio_parameter>(false, chans, node));

    // TODO update
  }
  else if (kind == "out")
  {
    auto chans = ossia::any_cast<ossia::audio_mapping>(attr.at("audio-mapping"));
    if (!node.get_parameter())
      node.set_parameter(std::make_unique<ossia::mapped_audio_parameter>(true, chans, node));
  }
  else if (kind == "virtual")
  {
    auto chans = ossia::any_cast<int>(attr.at("audio-channels"));
    if (!node.get_parameter())
      node.set_parameter(std::make_unique<ossia::virtual_audio_parameter>(chans, node));
  }

  auto x = node.get_extended_attributes();
  for (auto& e : attr)
  {
    x[e.first] = e.second;
  }
  node.set_extended_attributes(x);
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

Device::DeviceEnumerator* AudioProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface* AudioProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  auto doc = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if (doc)
  {
    auto cur = doc->list().audioDevice();
    if (cur)
    {
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
    s.name = "Audio";
    return s;
  }();
  return settings;
}

class AudioAddressDialog final : public Device::AddressDialog
{
public:
  void updateType(int idx)
  {
    auto& set = score::AppContext().settings<Audio::Settings::Model>();
    updateType(idx, set.getDefaultIn(), set.getDefaultOut());
  }
  void init()
  {
    // Make two things: possibility to add a virtual port for which each
    // channels maps to an existing channel And a pure address-only port used
    // for inter-transfer.
    this->setMinimumWidth(500);
    m_layout.addRow(tr("Name"), &m_nameEdit);
    m_layout.addRow(tr("Channels"), &m_channels);
    m_layout.addRow(tr("Type"), &m_type);
    m_layout.addRow(tr("Mapping\nRows: this parameter\nCols: sound card"), &m_mapping);
    m_layout.addWidget(&m_buttons);

    connect(&m_buttons, &QDialogButtonBox::accepted, this, &AudioAddressDialog::accept);
    connect(&m_buttons, &QDialogButtonBox::rejected, this, &AudioAddressDialog::reject);

    m_type.addItems({tr("In"), tr("Out"), tr("Virtual")});
    m_channels.setRange(1, 127);
    m_channels.setValue(2);
    m_type.setCurrentIndex(0);

    con(
        m_type,
        SignalUtils::QComboBox_currentIndexChanged_int(),
        this,
        [=] { updateType(m_type.currentIndex()); },
        Qt::QueuedConnection);
    con(
        m_channels,
        &QSpinBox::editingFinished,
        this,
        [=] { updateType(m_type.currentIndex()); },
        Qt::QueuedConnection);

    m_mapping.setAlternatingRowColors(true);
    m_mapping.setCornerButtonEnabled(false);
    m_mapping.horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_mapping.horizontalHeader()->setSectionsClickable(false);
    m_mapping.horizontalHeader()->setSectionsMovable(false);
    m_mapping.horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_mapping.verticalHeader()->setSectionsClickable(false);
    m_mapping.verticalHeader()->setSectionsMovable(false);
    m_mapping.verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    updateType(m_type.currentIndex());
  }
  AudioAddressDialog(
      Dataflow::AudioDevice& dev,
      const score::DocumentContext& ctx,
      QWidget* parent)
      : Device::AddressDialog{parent}
      , m_device{dev}
      , m_layout{this}
      , m_nameEdit{this}
      , m_type{this}
      , m_channels{this}
      , m_mapping{this}
      , m_buttons{
            QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel,
            this}
  {
    init();
  }

  AudioAddressDialog(
      const Device::AddressSettings& addr,
      Dataflow::AudioDevice& dev,
      const score::DocumentContext& ctx,
      QWidget* parent)
      : AudioAddressDialog{dev, ctx, parent}
  {
    m_nameEdit.setText(addr.name);
    {
      auto it = addr.extendedAttributes.find("audio-channels");
      if (it == addr.extendedAttributes.end())
        return;
      auto chans = ossia::any_cast<int>(it->second);
      m_channels.setValue(chans);
    }
    {
      auto it = addr.extendedAttributes.find("audio-kind");
      if (it == addr.extendedAttributes.end())
        return;

      auto kind = ossia::any_cast<std::string>(it->second);
      if (kind == "in")
        m_type.setCurrentIndex(0);
      else if (kind == "out")
        m_type.setCurrentIndex(1);
      else if (kind == "virtual")
        m_type.setCurrentIndex(2);

      updateType(m_type.currentIndex());
    }
    QCoreApplication::processEvents();
    {
      auto it = addr.extendedAttributes.find("audio-mapping");
      if (it == addr.extendedAttributes.end())
        return;
      const ossia::audio_mapping& mpng = ossia::any_cast<ossia::audio_mapping>(it->second);

      for (std::size_t i = 0; i < m_checkboxes.size(); i++)
      {
        for (std::size_t j = 0; j < mpng.size(); j++)
        {
          if (mpng[j] == (int)i && j < m_checkboxes[i].size())
          {
            m_checkboxes[i][j]->setChecked(true);
          }
        }
      }
    }
  }

  void updateType(int k, int inputs, int outputs)
  {
    for (auto g : m_groups)
      delete g;
    m_groups.clear();

    m_checkboxes.clear();
    m_mapping.clear();
    auto dev = m_device.getDevice();
    SCORE_ASSERT(dev);
    // ideally here we need a matrix that goes from "number of channels" to
    // "number of inputs"

    switch (k)
    {
      case 0:
      {
        m_mapping.setColumnCount(inputs);
        m_mapping.setRowCount(m_channels.value());
        m_groups.resize(m_channels.value());
        m_checkboxes.resize(inputs);
        for (auto& c : m_checkboxes)
          c.resize(m_channels.value());

        for (int j = 0; j < m_channels.value(); j++)
        {
          for (int i = 0; i < inputs; i++)
          {
            auto cb = new QRadioButton{&m_mapping};
            m_mapping.setCellWidget(j, i, cb);
            m_checkboxes[i][j] = cb;
          }
        }
        for (int j = 0; j < m_channels.value(); j++)
        {
          auto g = new QButtonGroup;
          g->setExclusive(true);
          m_groups.push_back(g);
          for (int i = 0; i < inputs; i++)
          {
            auto cb = m_checkboxes[i][j];
            cb->setChecked(i == j);
            g->addButton(cb, i);
          }
        }
        break;
      }
      case 1:
      {
        m_mapping.setColumnCount(outputs);
        m_mapping.setRowCount(m_channels.value());
        m_groups.resize(m_channels.value());
        m_checkboxes.resize(outputs);
        for (auto& c : m_checkboxes)
          c.resize(m_channels.value());

        for (int j = 0; j < m_channels.value(); j++)
        {
          auto g = new QButtonGroup;
          g->setExclusive(true);
          m_groups.push_back(g);
          for (int i = 0; i < outputs; i++)
          {
            auto cb = new QRadioButton{};
            g->addButton(cb, i);
            cb->setChecked(i == 0);
            m_mapping.setCellWidget(j, i, cb);
            m_checkboxes[i][j] = cb;
          }
        }
        break;
      }
      case 2:
        m_mapping.setColumnCount(0);
        m_mapping.setRowCount(0);
        break;
    }
  }

  Device::AddressSettings getSettings() const override
  {
    Device::AddressSettings addr;
    addr.name = m_nameEdit.text();
    addr.value = ossia::value{};
    switch (m_type.currentIndex())
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

    if (!m_checkboxes.empty())
    {
      ossia::audio_mapping mpng;
      mpng.resize(m_checkboxes[0].size());
      for (std::size_t i = 0; i < m_checkboxes.size(); i++)
      {
        for (std::size_t j = 0; j < m_checkboxes[i].size(); j++)
        {
          if (m_checkboxes[i][j]->isChecked())
          {
            mpng[j] = i; // local channel j goes to global channel i
          }
        }
      }
      addr.extendedAttributes["audio-mapping"] = std::move(mpng);
    }
    return addr;
  }

  AudioDevice& m_device;

  QFormLayout m_layout;
  State::AddressFragmentLineEdit m_nameEdit;
  QComboBox m_type;
  QSpinBox m_channels;
  QTableWidget m_mapping;
  QDialogButtonBox m_buttons;
  std::vector<QButtonGroup*> m_groups;
  std::vector<std::vector<QRadioButton*>> m_checkboxes;
};

Device::AddressDialog* AudioProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return new AudioAddressDialog{(AudioDevice&)safe_cast<const AudioDevice&>(dev), ctx, parent};
}

Device::AddressDialog* AudioProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return new AudioAddressDialog{
      set, (AudioDevice&)safe_cast<const AudioDevice&>(dev), ctx, parent};
}

Device::ProtocolSettingsWidget* AudioProtocolFactory::makeSettingsWidget()
{
  return new AudioSettingsWidget;
}

QVariant AudioProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return {};
}

void AudioProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
}

bool AudioProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

AudioSettingsWidget::AudioSettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

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
