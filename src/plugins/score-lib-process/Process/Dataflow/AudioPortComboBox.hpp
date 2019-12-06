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

  QString addressString() const;

public:
  void addressChanged(const Device::FullAddressSettings& arg_1)
      W_SIGNAL(addressChanged, arg_1)

private:
  void customContextMenuEvent(const QPoint& p);

  const State::Address m_root;
  Device::FullAddressSettings m_address;
  std::vector<QString> m_child;
};
}
