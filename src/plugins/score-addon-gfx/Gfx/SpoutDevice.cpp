#include "SpoutDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::SpoutDevice)

#include <QTimer>
namespace Gfx
{

  QTimer* timer_unsafe{};

SpoutNode::SpoutNode()
  : OutputNode{}
{
  input.push_back(new Port{this, {}, Types::Image, {}});
  timer_unsafe = new QTimer;
  QObject::connect(timer_unsafe, &QTimer::timeout,
                   [this] {
    if(m_renderer) {
      m_renderer->render();
    }
  });
}

SpoutNode::~SpoutNode()
{

}
bool SpoutNode::canRender() const
{
  return bool(timer_unsafe);
}

void SpoutNode::startRendering()
{
  timer_unsafe->start(16);
  /*
  if (window)
  {
    window->onRender = [this] {
    if (auto r = window->state.renderer)
    {
      window->canRender = r->renderedNodes.size() > 1;
      r->render();
    }
    };
  }*/
}

void SpoutNode::onRendererChange()
{/*
  if (window)
    if (auto r = window->state.renderer)
      window->canRender = r->renderedNodes.size() > 1;
      */
}

void SpoutNode::stopRendering()
{
  timer_unsafe->stop();
  /*
  if (window)
  {
    window->canRender = false;
    window->onRender = [] {};
    ////window->state.hasSwapChain = false;
  }
  */
}

void SpoutNode::setRenderer(Renderer* r)
{
  m_renderer = r;
}

Renderer* SpoutNode::renderer() const
{
  return m_renderer;
}

void SpoutNode::createOutput(
      GraphicsApi graphicsApi,
      std::function<void ()> onReady,
      std::function<void ()> onResize)
{
  window = std::make_shared<SpoutSender>();
  m_renderState = std::make_shared<RenderState>(RenderState::createOffscreen(graphicsApi));

  auto rhi = m_renderState->rhi;
  QRhiTexture *tex = rhi->newTexture(
                       QRhiTexture::RGBA8,
                       QSize(1280, 720),
                       1,
                       QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
  tex->build();
  QRhiTextureRenderTarget *rt = rhi->newTextureRenderTarget({ tex });
  QRhiRenderPassDescriptor *rp = rt->newCompatibleRenderPassDescriptor();
  rt->setRenderPassDescriptor(rp);
  rt->build();

  onReady();

  /*
  window = std::make_shared<Window>(graphicsApi);

#if QT_CONFIG(vulkan)
  if (graphicsApi == Vulkan)
    window->setVulkanInstance(staticVulkanInstance());
#endif
  window->onWindowReady = [this, graphicsApi, onReady] {
    window->state = RenderState::create(*window, graphicsApi);

    onReady();
  };
  window->onResize = onResize;
  window->resize(1280, 720);
  window->show();
  */
}

void SpoutNode::destroyOutput()
{
  window.reset();
}

RenderState* SpoutNode::renderState() const
{
  return m_renderState.get();
}

class SpoutRenderer : public RenderedNode
{
public:
  using RenderedNode::RenderedNode;
  void createRenderTarget(const RenderState& state) override
  {
    if(state.swapChain)
    {
      m_renderTarget = state.swapChain->currentFrameRenderTarget();
      m_renderPass = state.renderPassDescriptor;
    }
    else
    {
      m_renderTarget = nullptr;
      m_renderPass = nullptr;
      qDebug() << "Warning: swapchain not found in screenRenderTarget";
    }
  }
};

RenderedNode* SpoutNode::createRenderer() const noexcept
{
  return new SpoutRenderer{*this};
}





SpoutDevice::SpoutDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
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

SpoutDevice::~SpoutDevice() { }

QMimeData* SpoutDevice::mimeData() const
{
  auto mimeData = new QMimeData;

  State::Message mess;
  mess.address.address.device = m_settings.name;

  Mime<State::MessageList>::Serializer s{*mimeData};
  s.serialize({mess});
  return mimeData;
}

void SpoutDevice::setupContextMenu(QMenu& menu) const
{
}

void SpoutDevice::addAddress(const Device::FullAddressSettings& settings)
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

void SpoutDevice::updateAddress(
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

void SpoutDevice::disconnect()
{
  // TODO handle listening ??
  // setLogging_impl(Device::get_cur_logging(isLogging()));
}

bool SpoutDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if (plug)
    {
      m_protocol = new spout_protocol{plug->exec};
      m_dev = std::make_unique<spout_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol), "spout");
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

void SpoutDevice::recreate(const Device::Node& n)
{
  for (auto& child : n)
  {
    addNode(child);
  }
}

void SpoutDevice::setupNode(ossia::net::node_base& node, const ossia::extended_attributes& attr)
{
  // TODO
}

Device::Node SpoutDevice::refresh()
{
  return simple_refresh();
}

QString SpoutProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Spout Output");
}

QString SpoutProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator* SpoutProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface* SpoutProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new SpoutDevice(settings, ctx);
}

const Device::DeviceSettings& SpoutProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "spout_out";
    return s;
  }();
  return settings;
}

Device::AddressDialog* SpoutProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* SpoutProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* SpoutProtocolFactory::makeSettingsWidget()
{
  return new SpoutSettingsWidget;
}

QVariant SpoutProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return {};
}

void SpoutProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
}

bool SpoutProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

SpoutSettingsWidget::SpoutSettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

  setLayout(layout);

  setDefaults();
}

void SpoutSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("spout_out");
}

Device::DeviceSettings SpoutSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = SpoutProtocolFactory::static_concreteKey();
  return s;
}

void SpoutSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}

}
