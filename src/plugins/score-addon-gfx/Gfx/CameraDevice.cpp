#include "CameraDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::CameraDevice)

namespace Gfx
{

CameraDevice::CameraDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
    : DeviceInterface{settings}, m_ctx{ctx}
{
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canRefreshTree = true;
  m_capas.canRefreshValue = false;
  m_capas.hasCallbacks = false;
  m_capas.canListen = false;
  m_capas.canSerialize = true;
}

CameraDevice::~CameraDevice() { }

QMimeData* CameraDevice::mimeData() const
{
  auto mimeData = new QMimeData;

  State::Message mess;
  mess.address.address.device = m_settings.name;

  Mime<State::MessageList>::Serializer s{*mimeData};
  s.serialize({mess});
  return mimeData;
}

void CameraDevice::addAddress(const Device::FullAddressSettings& settings)
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

void CameraDevice::updateAddress(
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

void CameraDevice::disconnect()
{
  // TODO handle listening ??
  // setLogging_impl(Device::get_cur_logging(isLogging()));
}

bool CameraDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if (plug)
    {
      m_protocol = new camera_protocol{plug->exec};
      m_dev = std::make_unique<camera_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol), "camera");
    }
    // TODOengine->reload(&proto);

    // setLogging_impl(Device::get_cur_logging(isLogging()));
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

void CameraDevice::recreate(const Device::Node& n)
{
  for (auto& child : n)
  {
    addNode(child);
  }
}

void CameraDevice::setupNode(ossia::net::node_base& node, const ossia::extended_attributes& attr)
{
  // TODO
}

Device::Node CameraDevice::refresh()
{
  return simple_refresh();
}

QString CameraProtocolFactory::prettyName() const
{
  return QObject::tr("Gfx");
}

Device::DeviceInterface* CameraProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new CameraDevice(settings, ctx);
}

const Device::DeviceSettings& CameraProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Gfx";
    return s;
  }();
  return settings;
}

Device::AddressDialog* CameraProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* CameraProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* CameraProtocolFactory::makeSettingsWidget()
{
  return new CameraSettingsWidget;
}

QVariant CameraProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return {};
}

void CameraProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
}

bool CameraProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const
{
  return a.name != b.name;
}

CameraSettingsWidget::CameraSettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

  setLayout(layout);

  setDefaults();
}

void CameraSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("camera");
}

Device::DeviceSettings CameraSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  return s;
}

void CameraSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}
}
