#pragma once
#include <State/Address.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <Process/Dataflow/PortType.hpp>

#include <optional>

#include <score/document/DocumentContext.hpp>

#include <QComboBox>

#include <score_lib_process_export.h>

#include <verdigris>
namespace Device
{
class DeviceInterface;
class DeviceList;
}
namespace Process
{
class Port;
class SCORE_LIB_PROCESS_EXPORT AudioPortComboBox final : public QComboBox
{
  W_OBJECT(AudioPortComboBox)
public:
  AudioPortComboBox(
      const State::Address& rootAddress, const Device::Node& node, QWidget* parent);

  void setAddress(const State::Address& addr);

  const Device::FullAddressSettings& address() const;

  void addressChanged(const Device::FullAddressSettings& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, addressChanged, arg_1)

private:
  const State::Address m_root;
  Device::FullAddressSettings m_address;
  std::vector<QString> m_child;
};

//! The device a drop of these nodes names, for a port addressed by device.
//!
//! Audio, MIDI and texture ports hold a device and no path -- what their combo
//! box writes -- so dropping the device itself is the gesture that fits them.
//! A message port needs a parameter, and a device is not one.
SCORE_LIB_PROCESS_EXPORT
std::optional<State::Address> droppedDeviceAddress(
    const Device::FreeNodeList& nodes, Process::PortType type) noexcept;

SCORE_LIB_PROCESS_EXPORT
QComboBox* makeAddressCombo(
    State::Address root, const Device::Node& out_node, const Process::Port& port,
    const score::DocumentContext& ctx, QWidget* parent);

//! Devices that can stand at the other end of `port`.
//!
//! A kind rather than a predicate on the device object: a document whose score
//! runs elsewhere has no device objects at all, so a predicate could only ever
//! answer "none" there. A kind is a fact the machine that has the device can
//! report, and Explorer::DeviceDocumentPlugin holds what it reported.
SCORE_LIB_PROCESS_EXPORT
QComboBox* makeDeviceCombo(
    Device::DeviceKind kind, Device::DeviceList& devices, const Process::Port& port,
    const score::DocumentContext& ctx, QWidget* parent);
/*
class SCORE_LIB_PROCESS_EXPORT MidiPortComboBox final : public QComboBox
{
    W_OBJECT(MidiPortComboBox)
public:
  MidiPortComboBox(
      const std::vector<QString>& devices,
      QWidget* parent);

  void setDevice(const QString& dev);

  const QString& device() const;

  void deviceChanged(const QString& arg_1)
  W_SIGNAL(deviceChanged, arg_1)

private:
  QString m_device;
  std::vector<QString> m_available;
};*/
}
