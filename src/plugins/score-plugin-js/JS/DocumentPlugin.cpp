#include "DocumentPlugin.hpp"

#include <wobjectimpl.h>

MODEL_METADATA_IMPL_CPP(JS::DocumentPlugin)
namespace JS
{
DocumentPlugin::~DocumentPlugin() { }
DocumentPlugin::DocumentPlugin(const score::DocumentContext& ctx, QObject* parent)
    : SerializableDocumentPlugin{ctx, "JS::DocumentPlugin", parent}
{
}
}

template <>
void DataStreamReader::read(const JS::DocumentPlugin& plug)
{
  m_stream << plug.data;
  insertDelimiter();
}

template <>
void JSONReader::read(const JS::DocumentPlugin& plug)
{
  this->obj["Data"] = plug.data;
}

template <>
void DataStreamWriter::write(JS::DocumentPlugin& plug)
{
  m_stream >> plug.data;
  checkDelimiter();
}

template <>
void JSONWriter::write(JS::DocumentPlugin& plug)
{
  plug.data <<= this->obj["Data"];
}

W_OBJECT_IMPL(JS::DocumentPlugin)
