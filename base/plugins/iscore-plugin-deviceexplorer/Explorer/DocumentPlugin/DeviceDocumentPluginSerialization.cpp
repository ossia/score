#include "DeviceDocumentPlugin.hpp"
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <iscore/serialization/VariantSerialization.hpp>

template <>
void DataStreamReader::read(
    const Explorer::DeviceDocumentPlugin& dev)
{
  readFrom(dev.rootNode());
  insertDelimiter();
}


template <>
void JSONObjectReader::readFromConcrete(
    const Explorer::DeviceDocumentPlugin& plug)
{
  // Childrens of the root node are the devices
  // We don't save their children if they don't have canSerialize().

  obj["RootNode"] = QJsonObject{};
  QJsonArray children;
  for (const Device::Node& node : plug.rootNode().children())
  {
    QJsonObject this_node;

    ISCORE_ASSERT(node.is<Device::DeviceSettings>());
    const Device::DeviceSettings& dev = node.get<Device::DeviceSettings>();
    auto actual = plug.list().findDevice(dev.name);
    ISCORE_ASSERT(actual);
    if (actual->capabilities().canSerialize)
    {
      this_node = toJsonObject(node);
    }
    else
    {
      this_node = toJsonObject(node.impl());
    }

    children.push_back(std::move(this_node));
  }
  obj["Children"] = children;
}


template <>
void DataStreamWriter::writeTo(Explorer::DeviceDocumentPlugin& plug)
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
void JSONObjectWriter::writeTo(Explorer::DeviceDocumentPlugin& plug)
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
