#pragma once
#include <State/Address.hpp>

#include <Device/Protocol/DeviceInterface.hpp>

#include <Process/Dataflow/PortType.hpp>

#include <score/document/DocumentContext.hpp>

#include <QComboBox>

#include <functional>
#include <vector>

#include <score_lib_process_export.h>

#include <verdigris>

namespace Device
{
class DeviceList;
}
namespace Process
{
class Port;

/**
 * @brief The kind of device node a port of that type binds to.
 *
 * An inlet reads what a device produces, an outlet feeds what it consumes.
 */
SCORE_LIB_PROCESS_EXPORT
Device::NodeKind nodeKindOf(PortType type, bool inlet) noexcept;

/**
 * @brief The addresses of a device that a port of that type can bind to.
 *
 * The device is only walked when its capabilities say it carries nodes of
 * that kind (Device::NodeKind), so an OSC tree is never visited for textures.
 * Audio, texture and geometry nodes are recognized by their parameter type;
 * the audio device's mapped ports by their direction. A MIDI stream is the
 * device or one of its channel nodes: the note / CC leaves are values.
 */
SCORE_LIB_PROCESS_EXPORT
std::vector<State::Address>
listPortAddresses(Device::DeviceInterface& device, PortType type, bool inlet);

SCORE_LIB_PROCESS_EXPORT
std::vector<State::Address>
listPortAddresses(Device::DeviceList& devices, PortType type, bool inlet);

/**
 * @brief Address picker for the audio / midi / texture / geometry ports.
 *
 * An editable combo box: the drop-down lists the addresses of
 * listPortAddresses(), kept up to date with the devices; an address that is
 * not listed (device not connected yet, node created at run time...) can be
 * typed or dropped from the device explorer.
 */
class SCORE_LIB_PROCESS_EXPORT PortAddressComboBox final : public QComboBox
{
  W_OBJECT(PortAddressComboBox)
public:
  PortAddressComboBox(
      Device::DeviceList& devices, PortType type, bool inlet, QWidget* parent);
  ~PortAddressComboBox() override;

  void setAddress(const State::AddressAccessor& addr);
  const State::AddressAccessor& address() const noexcept { return m_address; }

  //! Rebuilds the drop-down from the devices.
  void reload();

  //! Devices the machine running the score reported as carrying this kind.
  //! A terminal holds no device objects to walk, so without this its list is
  //! empty and there is nothing to pick. Empty for an ordinary document.
  std::function<std::vector<QString>()> remoteDevices;

  void addressChanged(const State::AddressAccessor& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, addressChanged, arg_1)

private:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

  void watch(Device::DeviceInterface& dev);
  void scheduleReload();
  void showAddress();
  void commitText();
  void commit(State::AddressAccessor addr);

  Device::DeviceList& m_devices;
  State::AddressAccessor m_address;
  PortType m_type{};
  bool m_inlet{};
  bool m_reloadPending{};
};

/**
 * @brief A PortAddressComboBox bound to a port: follows the port's address and
 * submits a ChangePortAddress command when the user picks another one.
 */
SCORE_LIB_PROCESS_EXPORT
QComboBox* makePortAddressCombo(
    const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent);
}
