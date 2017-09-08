#pragma once

/**
 * \namespace Device
 * \brief Manipulation of Devices from Qt.
 *
 * This namespace contains tools to work with the score node tree.
 *
 * The problem is as follows :
 * We want to display the nodes defined in the OSSIA API (\ref
 * ossia::net::node_base)
 * in a QTreeView.
 *
 * QTreeView's model must inherit from QAbstractItemModel. The Qt documentation
 * explains how to build such a model.
 * The idea is that the model requires a data structure that it will control
 * entirely :
 * one cannot change the data structure from the outisde, edition has to go
 * through the QAbstractItemModel class to notify the view of changes of the
 * model.
 *
 * The QAbstractItemModel is implemented in Explorer::DeviceExplorerModel.
 *
 * But since we do not control the network messages that change the OSSIA nodes
 * automatically,
 * we have to create another set of classes that will mirror them and be used
 * solely for
 * displaying and editing by the QAbstractItemModel.
 *
 * This is the role of the class Device::Node.
 *
 * * The class Device::DeviceInterface wraps an ossia::net::device_base.
 * * When a node of the device changes, the Device::DeviceInterface is notified
 * and sends a signal.
 * * The class Explorer::DeviceDocumentPlugin with Explorer::NodeUpdateProxy
 * does the conversion
 *   from Device::DeviceInterface's signals to actions on the
 * Explorer::DeviceExplorerModel,
 *   which applies the changes to the device tree to the Device::Node
 * hierarchy.
 *
 * Likewise, when loading a scenario, first the Device::Node hierarchy is
 * deserialized,
 * then the relevant ossia data structures are recreated.
 *
 * This library also contains the relevant data-only structures used for
 * serialization:
 * Device::AddressSettings, etc., and interfaces for the UIs of the protocols.
 *
 */
