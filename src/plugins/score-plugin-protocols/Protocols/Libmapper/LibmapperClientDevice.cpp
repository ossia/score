#include "LibmapperClientDevice.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/protocols/libmapper/libmapper_protocol.hpp>

#include <mapper/mapper.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::LibmapperClientDevice)

namespace Protocols
{

LibmapperClientDevice::LibmapperClientDevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

LibmapperClientDevice::~LibmapperClientDevice() { }

bool LibmapperClientDevice::reconnect()
{
  disconnect();

  auto stgs = settings().deviceSpecificSettings.value<LibmapperClientSpecificSettings>();
  if(!stgs.id.isEmpty())
  {
    try
    {
      m_dev = std::make_unique<ossia::net::generic_device>(
          std::make_unique<ossia::net::libmapper_client_protocol>(stgs.id.toStdString()),
          settings().name.toStdString());
      m_dev->get_protocol().update(m_dev->get_root_node());
      deviceChanged(nullptr, m_dev.get());
    }
    catch(...)
    {
      SCORE_TODO;
    }
  }

  return connected();
}

void LibmapperClientDevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}
}

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <QObject>
namespace Protocols
{

class LibmapperClientEnumerator : public Device::DeviceEnumerator
{
public:
  LibmapperClientEnumerator()
  {
    const auto MPR_OBJ_ALL = MPR_OBJ_NEW | MPR_OBJ_MOD | MPR_OBJ_REM | MPR_OBJ_EXP;
    m_db = mpr_graph_new(MPR_OBJ_ALL);
    mpr_graph_add_cb(m_db, scan_event, MPR_OBJ_ALL, this);

    startTimer(100);
  }

  void timerEvent(QTimerEvent*) override { mpr_graph_poll(m_db, 100); }

  static void
  scan_event(mpr_graph db, mpr_obj obj, const mpr_graph_evt event, const void* user)
  {
    auto self = (LibmapperClientEnumerator*)user;

    switch(mpr_obj_get_type(obj))
    {
      case MPR_DEV:
        break;
      case MPR_SIG:
      default:
        return;
    }

    switch(event)
    {
      case MPR_OBJ_NEW:
        self->add_device(obj);
        break;
      case MPR_OBJ_MOD:
        break;
      case MPR_OBJ_REM:
        self->remove_device(obj);
        break;
      case MPR_OBJ_EXP:
        break;
    }
  }

  void add_device(mpr_obj obj)
  {
    auto name = mpr_obj_get_prop_as_str(obj, MPR_PROP_NAME, nullptr);
    if(m_devices.count(name) == 0)
    {
      m_devices.insert(name);
      deviceAdded(settingsForInstance(name));
    }
  }

  void remove_device(mpr_obj obj)
  {
    auto name = mpr_obj_get_prop_as_str(obj, MPR_PROP_NAME, nullptr);

    if(m_devices.count(name) == 1)
    {
      m_devices.erase(name);
      deviceRemoved(QString::fromStdString(name));
    }
  }

  Device::DeviceSettings settingsForInstance(const std::string& instance) const noexcept
  {
    using namespace std::literals;

    Device::DeviceSettings set;
    set.name = QString::fromStdString(instance);
    set.protocol = LibmapperClientProtocolFactory::static_concreteKey();
    set.deviceSpecificSettings = QVariant::fromValue(
        LibmapperClientSpecificSettings{QString::fromStdString(instance)});
    return set;
  }

  void enumerate(std::function<void(const Device::DeviceSettings&)> f) const override { }

private:
  ossia::flat_set<std::string> m_devices;
  mpr_graph m_db{};
};

QString LibmapperClientProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("LibmapperClient");
}

QString LibmapperClientProtocolFactory::category() const noexcept
{
  return StandardCategories::hardware;
}

Device::DeviceEnumerator*
LibmapperClientProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return new LibmapperClientEnumerator;
}

Device::DeviceInterface* LibmapperClientProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new LibmapperClientDevice{settings, plugin.networkContext()};
}

const Device::DeviceSettings&
LibmapperClientProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings& settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "LibmapperClient";
    LibmapperClientSpecificSettings settings;
    s.deviceSpecificSettings = QVariant::fromValue(settings);
    return s;
  }();

  return settings;
}

Device::ProtocolSettingsWidget* LibmapperClientProtocolFactory::makeSettingsWidget()
{
  return new LibmapperClientProtocolSettingsWidget;
}

QVariant LibmapperClientProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<LibmapperClientSpecificSettings>(visitor);
}

void LibmapperClientProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<LibmapperClientSpecificSettings>(data, visitor);
}

bool LibmapperClientProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}

}
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/widgets/ComboBox.hpp>

#include <QFormLayout>
#include <QPushButton>
#include <QVariant>
#include <qlineedit.h>

namespace Protocols
{
LibmapperClientProtocolSettingsWidget::LibmapperClientProtocolSettingsWidget(
    QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);
  m_deviceNameEdit->setText("Joystick");

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);

  setLayout(layout);
}

LibmapperClientProtocolSettingsWidget::~LibmapperClientProtocolSettingsWidget() { }

Device::DeviceSettings LibmapperClientProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = LibmapperClientProtocolFactory::static_concreteKey();
  return s;
}

void LibmapperClientProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);
}
}
W_OBJECT_IMPL(Protocols::LibmapperClientProtocolSettingsWidget)

template <>
void DataStreamReader::read(const Protocols::LibmapperClientSpecificSettings& n)
{
  m_stream << n.id;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::LibmapperClientSpecificSettings& n)
{
  m_stream >> n.id;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::LibmapperClientSpecificSettings& n)
{
  obj["Id"] = n.id;
}

template <>
void JSONWriter::write(Protocols::LibmapperClientSpecificSettings& n)
{
  n.id <<= obj["Id"];
}
