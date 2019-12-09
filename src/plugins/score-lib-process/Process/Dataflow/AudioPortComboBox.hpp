#pragma once
#include <State/Address.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <QComboBox>
#include <verdigris>
#include <score_lib_process_export.h>
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT AudioPortComboBox final : public QComboBox
{
  W_OBJECT(AudioPortComboBox)
public:
  AudioPortComboBox(
          const State::Address& rootAddress,
          const Device::Node& node,
          QWidget* parent);

  void setAddress(const State::Address& addr);

  const Device::FullAddressSettings& address() const;

  void addressChanged(const Device::FullAddressSettings& arg_1)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, addressChanged, arg_1)

private:
  const State::Address m_root;
  Device::FullAddressSettings m_address;
  std::vector<QString> m_child;
};
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
