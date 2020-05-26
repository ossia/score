#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <State/Address.hpp>

#include <QComboBox>

#include <score_lib_process_export.h>

#include <verdigris>
namespace Process
{
class Port;
class SCORE_LIB_PROCESS_EXPORT AudioPortComboBox final : public QComboBox
{
  W_OBJECT(AudioPortComboBox)
public:
  AudioPortComboBox(const State::Address& rootAddress, const Device::Node& node, QWidget* parent);

  void setAddress(const State::Address& addr);

  const Device::FullAddressSettings& address() const;

  void addressChanged(const Device::FullAddressSettings& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, addressChanged, arg_1)

private:
  const State::Address m_root;
  Device::FullAddressSettings m_address;
  std::vector<QString> m_child;
};

SCORE_LIB_PROCESS_EXPORT
QWidget* makeAddressCombo(
    State::Address root,
    const Device::Node& out_node,
    const Process::Port& port,
    const score::DocumentContext& ctx,
    QWidget* parent);

SCORE_LIB_PROCESS_EXPORT
QWidget* makeDeviceCombo(
    QStringList devices,
    const Process::Port& port,
    const score::DocumentContext& ctx,
    QWidget* parent);
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
