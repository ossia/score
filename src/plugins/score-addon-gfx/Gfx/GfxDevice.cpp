#include "GfxDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::GfxDevice)

namespace Gfx
{

GfxDevice::GfxDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
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

GfxDevice::~GfxDevice() { }

QMimeData* GfxDevice::mimeData() const
{
  auto mimeData = new QMimeData;

  State::Message mess;
  mess.address.address.device = m_settings.name;

  Mime<State::MessageList>::Serializer s{*mimeData};
  s.serialize({mess});
  return mimeData;
}

void GfxDevice::setupContextMenu(QMenu& menu) const
{
  if (m_dev)
  {
    auto p = m_dev.get()->get_root_node().get_parameter();
    if (p)
    {
      if (auto s = p->screen)
      {
        if (auto w = s->window)
        {
          auto showhide = new QAction;
          if (!w->isVisible())
          {
            showhide->setText(tr("Show"));
            connect(showhide, &QAction::triggered, w.get(), [w] { w->show(); });
          }
          else
          {
            showhide->setText(tr("Hide"));
            connect(showhide, &QAction::triggered, w.get(), [w] { w->hide(); });
          }
          menu.addAction(showhide);
        }
      }
    }
  }
}

void GfxDevice::addAddress(const Device::FullAddressSettings& settings)
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

void GfxDevice::updateAddress(
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

void GfxDevice::disconnect()
{
  // TODO handle listening ??
  // setLogging_impl(Device::get_cur_logging(isLogging()));
}

bool GfxDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if (plug)
    {
      m_protocol = new gfx_protocol{plug->exec};
      m_dev = std::make_unique<gfx_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol), "gfx");
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

void GfxDevice::recreate(const Device::Node& n)
{
  for (auto& child : n)
  {
    addNode(child);
  }
}

void GfxDevice::setupNode(ossia::net::node_base& node, const ossia::extended_attributes& attr)
{
  // TODO
}

Device::Node GfxDevice::refresh()
{
  return simple_refresh();
}

QString GfxProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Gfx");
}

QString GfxProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator* GfxProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface* GfxProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new GfxDevice(settings, ctx);
}

const Device::DeviceSettings& GfxProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Gfx";
    return s;
  }();
  return settings;
}

Device::AddressDialog* GfxProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* GfxProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* GfxProtocolFactory::makeSettingsWidget()
{
  return new GfxSettingsWidget;
}

QVariant GfxProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return {};
}

void GfxProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
}

bool GfxProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

GfxSettingsWidget::GfxSettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

  setLayout(layout);

  setDefaults();
}

void GfxSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("gfx");
}

Device::DeviceSettings GfxSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = GfxProtocolFactory::static_concreteKey();
  return s;
}

void GfxSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}

}
