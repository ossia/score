#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <State/Value.hpp>

#include <QString>

#include <score_plugin_deviceexplorer_export.h>

namespace State
{
struct Address;
}

namespace Device
{
struct AddressSettings;
struct DeviceSettings;
class DeviceInterface;
}

namespace Explorer
{
class DeviceDocumentPlugin;
class DeviceExplorerModel;
/**
 * @brief The NodeUpdateProxy class
 *
 * This class is used to correctly add and
 * remove elements to the node hierarchy.
 * If there is no device explorer model yet (e.g. while restoring from a
 * crash),
 * we can simply add them where required
 * to the root node and they will be loaded afterwards.
 * But when there is a device explorer model, we have
 * to use its methods (that calls beginAddRows, etc.) to prevent a full refresh
 * which would be laggy.
 */
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT NodeUpdateProxy
{
public:
  DeviceDocumentPlugin& devModel;

  explicit NodeUpdateProxy(DeviceDocumentPlugin& root);
  NodeUpdateProxy(const NodeUpdateProxy&) = delete;
  NodeUpdateProxy(NodeUpdateProxy&&) = delete;
  NodeUpdateProxy& operator=(const NodeUpdateProxy&) = delete;
  NodeUpdateProxy& operator=(NodeUpdateProxy&&) = delete;

  void addDevice(const Device::DeviceSettings& dev);
  void loadDevice(const Device::Node& node);

  void updateDevice(const QString& name, const Device::DeviceSettings& dev);

  void removeDevice(const Device::DeviceSettings& dev);

  /**
   * @brief addAddress Adds a single address in the tree
   * @param parentPath Path to the parent
   * @param settings Informations of the address
   *
   * Used to add a new address (after input from user for instance)
   */
  void
  addAddress(const Device::NodePath& parentPath, const Device::AddressSettings& settings, int row);

  void addAddress(const Device::FullAddressSettings& settings);

  /**
   * @brief addNode Adds a node hierarchy in the tree
   * @param parentPath Path to the parent
   * @param node The node to insert.
   */
  void addNode(const Device::NodePath& parentPath, const Device::Node& node, int row);

  void updateAddress(const Device::NodePath& nodePath, const Device::AddressSettings& settings);

  void removeNode(const Device::NodePath& parentPath, const Device::AddressSettings& settings);

  // Local : the Device::Node structure
  // Remote : what's behind a DeviceInterface
  void addLocalAddress(Device::Node& parent, const Device::AddressSettings& data, int row);

  void addLocalNode(Device::Node& parent, Device::Node&& node);

  void removeLocalNode(const State::Address&);
  void updateLocalValue(const State::AddressAccessor&, const ossia::value&);
  void updateLocalSettings(
      const State::Address&,
      const Device::AddressSettings&,
      Device::DeviceInterface& newdev);

  void updateRemoteValue(const State::Address&, const ossia::value&);

  ossia::value refreshRemoteValue(const State::Address&) const;
  optional<ossia::value> try_refreshRemoteValue(const State::Address&) const;
  void refreshRemoteValues(const Device::NodeList&);

private:
  void rec_addNode(Device::NodePath parentPath, const Device::Node& node, int row);
};
}
