#include "TexturePort.hpp"

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
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Gfx::TextureInlet>(const Gfx::TextureInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Gfx::TextureInlet>(Gfx::TextureInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read<Gfx::TextureInlet>(const Gfx::TextureInlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Gfx::TextureInlet>(Gfx::TextureInlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Gfx::TextureOutlet>(const Gfx::TextureOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Gfx::TextureOutlet>(Gfx::TextureOutlet& p)
{
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read<Gfx::TextureOutlet>(const Gfx::TextureOutlet& p)
{
  // read((Process::Outlet&)p);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Gfx::TextureOutlet>(Gfx::TextureOutlet& p)
{
}
