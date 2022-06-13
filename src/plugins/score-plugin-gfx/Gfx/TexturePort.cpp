#include "TexturePort.hpp"

#include "GfxDevice.hpp"
#include <Gfx/GfxApplicationPlugin.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/Dataflow/AudioPortComboBox.hpp>
#include <QHBoxLayout>
#include <QPainter>
#include <QTimer>
#include <score/plugins/SerializableHelpers.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::TextureInlet)
W_OBJECT_IMPL(Gfx::TextureOutlet)

namespace Gfx
{
MODEL_METADATA_IMPL_CPP(TextureInlet);
MODEL_METADATA_IMPL_CPP(TextureOutlet);

class GraphPreviewWidget
    : public QWidget
{
public:
  GraphPreviewWidget(const TextureOutlet& outlet, Gfx::DocumentPlugin& plug)
    : outlet_p{&outlet}
    , plug{plug}
  {
    setLayout(new Inspector::VBoxLayout{this});
    auto window = std::make_unique<score::gfx::ScreenNode>(true);
    node = window.get();
    screenId = plug.context.register_preview_node(std::move(window));
    if(screenId != -1)
    {
      timerId = startTimer(16);
    }
  }

  void timerEvent(QTimerEvent*)
  {
    const auto& w = node->window();
    if(!w)
      return;

    if(!outlet_p)
      return;

    auto& outlet = *outlet_p;

    if(outlet.nodeId != nodeId)
    {
      if(e)
      {
        plug.context.disconnect_preview_node(*e);
        e = std::nullopt;
      }

      if(outlet.nodeId != -1)
      {
        nodeId = outlet.nodeId;
        e = { {nodeId, 0}, {screenId, 0} };
        plug.context.connect_preview_node(*e);
      }
    }

    if(!container)
    {
      qwindow = w.get();
      this->window = w;

      container = QWidget::createWindowContainer(qwindow, this);
      container->setMinimumWidth(100);
      container->setMaximumWidth(300);
      container->setMinimumHeight(200);
      container->setMaximumHeight(200);
      this->layout()->addWidget(container);
    }
  }

  ~GraphPreviewWidget()
  {
    if(qwindow)
    {
      // Take back ownership of the window
      qwindow->setParent(nullptr);
      QChildEvent ev(QEvent::ChildRemoved, qwindow);
      ((QObject*)container)->event(&ev);
    }

    // We "garbage collect" the window
    QTimer::singleShot(1, [w=this->window] { });
    plug.context.unregister_preview_node(screenId);
  }

private:
  QPointer<const TextureOutlet> outlet_p;
  Gfx::DocumentPlugin& plug;
  score::gfx::ScreenNode* node{};
  std::optional<Gfx::Edge> e;

  std::shared_ptr<score::gfx::Window> window;

  QWindow* qwindow{};
  QWidget* container{};

  int screenId = -1;
  int nodeId = -1;
  int timerId{};

};

TextureInlet::~TextureInlet() { }

TextureInlet::TextureInlet(Id<Process::Port> c, QObject* parent)
    : Process::Inlet{std::move(c), parent}
{
}

TextureInlet::TextureInlet(DataStream::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureInlet::TextureInlet(JSONObject::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureInlet::TextureInlet(DataStream::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureInlet::TextureInlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Inlet{vis, parent}
{
  vis.writeTo(*this);
}

TextureOutlet::~TextureOutlet() { }

TextureOutlet::TextureOutlet(Id<Process::Port> c, QObject* parent)
    : Process::Outlet{std::move(c), parent}
{
}

TextureOutlet::TextureOutlet(DataStream::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureOutlet::TextureOutlet(JSONObject::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureOutlet::TextureOutlet(DataStream::Deserializer&& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureOutlet::TextureOutlet(JSONObject::Deserializer&& vis, QObject* parent)
    : Outlet{vis, parent}
{
  vis.writeTo(*this);
}

void TextureInletFactory::setupInletInspector(
    const Process::Inlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  QStringList devices;
  devices.push_back("");

  device.list().apply([&](Device::DeviceInterface& dev) {
    if (dynamic_cast<GfxInputDevice*>(&dev))
    {
      auto& set = dev.settings();
      devices.push_back(set.name);
    }
  });

  lay.addRow(Process::makeDeviceCombo(devices, port, ctx, parent));
}

void TextureOutletFactory::setupOutletInspector(
    const Process::Outlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  auto& device = *ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  QStringList devices;
  devices.push_back("");

  device.list().apply([&](Device::DeviceInterface& dev) {
    if (dynamic_cast<GfxOutputDevice*>(&dev))
    {
      auto& set = dev.settings();
      devices.push_back(set.name);
    }
  });

  lay.addRow(Process::makeDeviceCombo(devices, port, ctx, parent));


  auto& outlet = safe_cast<const TextureOutlet&>(port);
  lay.addRow(new GraphPreviewWidget{outlet, ctx.plugin<Gfx::DocumentPlugin>()});
}
}

template <>
void DataStreamReader::read<Gfx::TextureInlet>(const Gfx::TextureInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void DataStreamWriter::write<Gfx::TextureInlet>(Gfx::TextureInlet& p)
{
}

template <>
void JSONReader::read<Gfx::TextureInlet>(const Gfx::TextureInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void JSONWriter::write<Gfx::TextureInlet>(Gfx::TextureInlet& p)
{
}

template <>
void DataStreamReader::read<Gfx::TextureOutlet>(const Gfx::TextureOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void DataStreamWriter::write<Gfx::TextureOutlet>(Gfx::TextureOutlet& p)
{
}

template <>
void JSONReader::read<Gfx::TextureOutlet>(const Gfx::TextureOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
void JSONWriter::write<Gfx::TextureOutlet>(Gfx::TextureOutlet& p)
{
}
