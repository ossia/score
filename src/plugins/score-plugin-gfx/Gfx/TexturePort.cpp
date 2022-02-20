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

MODEL_METADATA_IMPL_CPP(TextureInlet)
MODEL_METADATA_IMPL_CPP(TextureOutlet)

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

  struct Wrapper
      : public QWidget
  {
    const TextureOutlet& outlet;
    Gfx::DocumentPlugin& plug;
    score::gfx::ScreenNode* node{};
    int screenId = -1;
    int nodeId = -1;
    std::optional<Gfx::GfxExecutionAction::edge> e;

    std::shared_ptr<score::gfx::Window> window;

    QWindow* qwindow{};
    QWidget* container{};

    int timerId{};

    Wrapper(const TextureOutlet& outlet, Gfx::DocumentPlugin& plug)
      : outlet{outlet}
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

      if(outlet.nodeId != nodeId)
      {
        if(e)
        {
          plug.exec.unsetFixedEdge(e->first, e->second);
          e = std::nullopt;
        }

        if(outlet.nodeId != -1)
        {
          nodeId = outlet.nodeId;
          e = { {nodeId, 0}, {screenId, 0} };
          plug.exec.setFixedEdge(e->first, e->second);
        }
      }

      if(!container)
      {
        qwindow = w.get();

        container = QWidget::createWindowContainer(qwindow, this);
        container->setMinimumWidth(100);
        container->setMaximumWidth(300);
        container->setMinimumHeight(200);
        container->setMaximumHeight(200);
        this->layout()->addWidget(container);
      }
    }

    ~Wrapper()
    {
      if(qwindow)
      {
        // Take back ownership of the window
        qwindow->setParent(nullptr);
        QChildEvent ev(QEvent::ChildRemoved, qwindow);
        ((QObject*)container)->event(&ev);
      }

      if(e)
        plug.exec.unsetFixedEdge(e->first, e->second);
      plug.context.unregister_preview_node(screenId);
    }
  };

  auto& outlet = safe_cast<const TextureOutlet&>(port);
  lay.addRow(new Wrapper{outlet, ctx.plugin<Gfx::DocumentPlugin>()});
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
