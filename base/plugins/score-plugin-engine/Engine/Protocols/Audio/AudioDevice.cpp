#include "AudioDevice.hpp"
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>
#include <QLineEdit>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QHeaderView>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Engine/score2OSSIA.hpp>
#include <ossia/dataflow/audio_parameter.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DeviceList.hpp>
#include <Engine/Protocols/Settings/Model.hpp>
#if __has_include(<pa_jack.h>) && !defined(_MSC_VER)
#include <pa_jack.h>
#endif
namespace Dataflow
{
AudioDevice::AudioDevice(
    const Device::DeviceSettings& settings)
  : OSSIADevice{settings}
  , m_protocol{new ossia::audio_protocol}
  , m_dev{std::unique_ptr<ossia::net::protocol_base>(m_protocol), "audio"}
{
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canRefreshTree = true;
  m_capas.canRefreshValue = false;
  m_capas.hasCallbacks = false;
  m_capas.canListen = false;
  m_capas.canSerialize = false;

  reconnect();
#if __has_include(<pa_jack.h>) && !defined(_MSC_VER)
    PaJack_SetClientName("i-score");
#endif
}

void AudioDevice::addAddress(const Device::FullAddressSettings& settings)
{
  using namespace ossia;
  if (auto dev = getDevice())
  {
    // Create the node. It is added into the device.
    ossia::net::node_base* node = Engine::score_to_ossia::createNodeFromPath(
          settings.address.path, *dev);
    SCORE_ASSERT(node);

    auto kind_it = settings.extendedAttributes.find("audio-kind");
    if(kind_it == settings.extendedAttributes.end())
      return; // it will be added automatically
    auto kind = ossia::any_cast<std::string>(kind_it.value());
    if(kind == "in")
    {
      auto chans = ossia::any_cast<ossia::audio_mapping>(settings.extendedAttributes.at("audio-mapping"));
      node->set_parameter(std::make_unique<ossia::mapped_audio_parameter>(false, chans, *node));
    }
    else if(kind == "out")
    {
      auto chans = ossia::any_cast<ossia::audio_mapping>(settings.extendedAttributes.at("audio-mapping"));
      node->set_parameter(std::make_unique<ossia::mapped_audio_parameter>(true, chans, *node));
    }
    else if(kind == "virtual")
    {
      auto chans = ossia::any_cast<int>(settings.extendedAttributes.at("audio-channels"));
      node->set_parameter(std::make_unique<ossia::virtual_audio_parameter>(chans, *node));
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
    //AudioSpecificSettings stgs
    //    = settings().deviceSpecificSettings.value<AudioSpecificSettings>();

    auto& set = score::AppContext().settings<Audio::Settings::Model>();
    auto& proto = static_cast<ossia::audio_protocol&>(m_dev.get_protocol());

    proto.rate = set.getRate();
    proto.bufferSize = set.getBufferSize();
    proto.card_in = set.getCardIn().toStdString();
    proto.card_out = set.getCardOut().toStdString();

    proto.reload();

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

  return connected();
}

void AudioDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
}

Device::Node AudioDevice::refresh()
{
  return simple_refresh();
}

QString AudioProtocolFactory::prettyName() const
{
  return QObject::tr("Audio");
}

Device::DeviceInterface* AudioProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
{
  auto doc = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if (doc)
  {
    auto cur = doc->list().audioDevice();
    if(cur)
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

const Device::DeviceSettings& AudioProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Audio";
    AudioSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}
class AudioAddressDialog final : public Device::AddAddressDialog
{
  public:
    AudioAddressDialog(
        const Device::DeviceSettings& dev,
        const score::DocumentContext& ctx,
        QWidget* parent)
      : Device::AddAddressDialog{parent}
      , m_device{*ctx.plugin<Engine::Execution::DocumentPlugin>().audio_device}
      , m_layout{this}
      , m_nameEdit{this}
      , m_type{this}
      , m_channels{this}
      , m_mapping{this}
      , m_buttons{QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel, this}
    {
      // Make two things: possibility to add a virtual port for which each channels maps
      // to an existing channel
      // And a pure address-only port used for inter-transfer.
      this->setMinimumWidth(500);
      m_layout.addRow(tr("Name"), &m_nameEdit);
      m_layout.addRow(tr("Channels"), &m_channels);
      m_layout.addRow(tr("Type"), &m_type);
      m_layout.addRow(tr("Mapping\nRows: this parameter\nCols: sound card"), &m_mapping);
      m_layout.addWidget(&m_buttons);

      connect(&m_buttons, &QDialogButtonBox::accepted,
              this, &AudioAddressDialog::accept);
      connect(&m_buttons, &QDialogButtonBox::rejected,
              this, &AudioAddressDialog::reject);

      m_type.addItems({tr("In"), tr("Out"), tr("Virtual")});
      m_channels.setRange(1, 127);
      m_channels.setValue(2);
      m_type.setCurrentIndex(0);

      con(m_type, SignalUtils::QComboBox_currentIndexChanged_int(), this,
          &AudioAddressDialog::updateType, Qt::QueuedConnection);
      con(m_channels, &QSpinBox::editingFinished, this,
          [=] { updateType(m_type.currentIndex()); }, Qt::QueuedConnection);

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

    void updateType(int k)
    {
      for(auto g : m_groups)
        delete g;
      m_groups.clear();

      m_checkboxes.clear();
      m_mapping.clear();
      auto dev = m_device.getDevice();
      SCORE_ASSERT(dev);
      auto& aproto = static_cast<ossia::audio_protocol&>(dev->get_protocol());
      // ideally here we need a matrix that goes from "number of channels" to "number of inputs"

      switch(k)
      {
        case 0:
        {
          m_mapping.setColumnCount(aproto.inputs);
          m_mapping.setRowCount(m_channels.value());
          m_groups.resize(m_channels.value());
          m_checkboxes.resize(aproto.inputs);
          for(auto& c : m_checkboxes) c.resize(m_channels.value());

          for(int j = 0; j < m_channels.value(); j++)
          {
            for(int i = 0; i < aproto.inputs; i++)
            {
              auto cb = new QRadioButton{&m_mapping};
              m_mapping.setCellWidget(j, i, cb);
              m_checkboxes[i][j] = cb;
            }
          }
          for(int j = 0; j < m_channels.value(); j++)
          {
            auto g = new QButtonGroup;
            g->setExclusive(true);
            m_groups.push_back(g);
            for(int i = 0; i < aproto.inputs; i++)
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
          m_mapping.setColumnCount(aproto.outputs);
          m_mapping.setRowCount(m_channels.value());
          m_groups.resize(m_channels.value());
          m_checkboxes.resize(aproto.outputs);
          for(auto& c : m_checkboxes) c.resize(m_channels.value());

          for(int j = 0; j < m_channels.value(); j++)
          {
            auto g = new QButtonGroup;
            g->setExclusive(true);
            m_groups.push_back(g);
            for(int i = 0; i < aproto.outputs; i++)
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

      ossia::audio_mapping mpng;
      for(std::size_t i = 0; i < m_checkboxes.size(); i++)
      {
        mpng.resize(m_checkboxes[i].size());
        for(std::size_t j = 0; j < m_checkboxes[i].size(); j++)
        {
          if(m_checkboxes[i][j]->isChecked())
            mpng[j] = i; // local channel j goes to global channel i
        }
      }
      addr.extendedAttributes["audio-mapping"] = std::move(mpng);
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

Device::AddAddressDialog* AudioProtocolFactory::makeAddAddressDialog(
    const Device::DeviceSettings& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return new AudioAddressDialog{dev, ctx, parent};
}

Device::ProtocolSettingsWidget* AudioProtocolFactory::makeSettingsWidget()
{
  return new AudioSettingsWidget;
}

QVariant AudioProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<AudioSpecificSettings>(visitor);
}

void AudioProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<AudioSpecificSettings>(data, visitor);
}

bool AudioProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const
{
  return a.name != b.name;
}


AudioSettingsWidget::AudioSettingsWidget(QWidget* parent)
  : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

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

  AudioSpecificSettings audio;
  audio.card = "default";
  audio.rate = 44100;
  audio.bufferSize = 64;

  s.deviceSpecificSettings = QVariant::fromValue(audio);
  return s;
}

void AudioSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  AudioSpecificSettings audio;
  if (settings.deviceSpecificSettings
      .canConvert<AudioSpecificSettings>())
  {
    audio = settings.deviceSpecificSettings
            .value<AudioSpecificSettings>();
  }
}

}


template <>
void DataStreamReader::read(
    const Dataflow::AudioSpecificSettings& n)
{
  m_stream << n.card << n.bufferSize << n.rate;
  insertDelimiter();
}


template <>
void DataStreamWriter::write(
    Dataflow::AudioSpecificSettings& n)
{
  m_stream >> n.card >> n.bufferSize >> n.rate;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Dataflow::AudioSpecificSettings& n)
{
  obj["Card"] = n.card;
  obj["BufferSize"] = n.bufferSize;
  obj["Rate"] = n.rate;
}


template <>
void JSONObjectWriter::write(
    Dataflow::AudioSpecificSettings& n)
{
  n.card = obj["Card"].toString();
  n.bufferSize = obj["BufferSize"].toInt();
  n.rate = obj["Rate"].toInt();
}

