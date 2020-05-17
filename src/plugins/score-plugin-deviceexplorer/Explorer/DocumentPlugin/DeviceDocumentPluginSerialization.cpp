// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceDocumentPlugin.hpp"

#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/VariantSerialization.hpp>

template <>
void DataStreamReader::read(const Explorer::DeviceDocumentPlugin& dev)
{
  readFrom(dev.rootNode());
  insertDelimiter();
}

template <>
void JSONReader::read(const Explorer::DeviceDocumentPlugin& plug)
{
  // Childrens of the root node are the devices
  // We don't save their children if they don't have canSerialize().

  // TODO WTF
  this->stream.Key("RootNode");
  this->stream.StartObject();
  this->stream.EndObject();

  this->stream.Key("Children");
  this->stream.StartArray();
  for (const Device::Node& node : plug.rootNode().children())
  {
    // TODO maybe objects shouldn't be there actually ?

    SCORE_ASSERT(node.is<Device::DeviceSettings>());
    const Device::DeviceSettings& dev = node.get<Device::DeviceSettings>();
    auto actual = plug.list().findDevice(dev.name);
    SCORE_ASSERT(actual);
    if (actual->capabilities().canSerialize)
    {
      this->readFrom(node);
    }
    else
    {
      this->readFrom(node.impl());
    }
  }
  this->stream.EndArray();
}

template <>
void DataStreamWriter::write(Explorer::DeviceDocumentPlugin& plug)
{
  Device::Node n;
  writeTo(n);
  checkDelimiter();

  plug.m_explorer = new Explorer::DeviceExplorerModel{plug, &plug};
  // Here everything is loaded in m_loadingNode

  // Here we recreate the correct structures in term of devices,
  // given what's present in the node hierarchy
  for (const auto& node : n)
  {
    plug.updateProxy.loadDevice(node);
  }
}

template <>
void JSONWriter::write(Explorer::DeviceDocumentPlugin& plug)
{
  Device::Node n;
  writeTo(n);

  plug.m_explorer = new Explorer::DeviceExplorerModel{plug, &plug};
  // Here everything is loaded in m_loadingNode

  // Here we recreate the correct structures in term of devices,
  // given what's present in the node hierarchy
  for (const auto& node : n)
  {
    plug.updateProxy.loadDevice(node);
  }
}
