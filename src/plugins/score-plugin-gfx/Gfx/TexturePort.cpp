#include "GfxDevice.hpp"
#include "TexturePort.hpp"

#include <Device/Protocol/DeviceInterface.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <Process/Dataflow/AudioPortComboBox.hpp>

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

TextureInlet::TextureInlet(DataStream::Deserializer& vis, QObject* parent) : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureInlet::TextureInlet(JSONObject::Deserializer& vis, QObject* parent) : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureInlet::TextureInlet(DataStream::Deserializer&& vis, QObject* parent) : Inlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureInlet::TextureInlet(JSONObject::Deserializer&& vis, QObject* parent) : Inlet{vis, parent}
{
  vis.writeTo(*this);
}

TextureOutlet::~TextureOutlet() { }

TextureOutlet::TextureOutlet(Id<Process::Port> c, QObject* parent)
    : Process::Outlet{std::move(c), parent}
{
}

TextureOutlet::TextureOutlet(DataStream::Deserializer& vis, QObject* parent) : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureOutlet::TextureOutlet(JSONObject::Deserializer& vis, QObject* parent) : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureOutlet::TextureOutlet(DataStream::Deserializer&& vis, QObject* parent) : Outlet{vis, parent}
{
  vis.writeTo(*this);
}
TextureOutlet::TextureOutlet(JSONObject::Deserializer&& vis, QObject* parent) : Outlet{vis, parent}
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
    if(dynamic_cast<GfxInputDevice*>(&dev))
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
    if(dynamic_cast<GfxOutputDevice*>(&dev))
    {
      auto& set = dev.settings();
      devices.push_back(set.name);
    }
  });

  lay.addRow(Process::makeDeviceCombo(devices, port, ctx, parent));
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
void
DataStreamReader::read<Gfx::TextureOutlet>(const Gfx::TextureOutlet& p)
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
