#pragma once

#include <Device/Protocol/DeviceSettings.hpp>

#include <QDialog>
#include <QList>

#include <score_plugin_deviceexplorer_export.h>

#include <verdigris>

class QComboBox;
class QFormLayout;
class QWidget;
class QListWidget;
class QTreeWidget;
class QLabel;
class QDialogButtonBox;

namespace Device
{
class ProtocolFactoryList;
class ProtocolSettingsWidget;
class DeviceEnumerator;
}
namespace Explorer
{
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceEditDialog final : public QDialog
{
  W_OBJECT(DeviceEditDialog)

public:
  explicit DeviceEditDialog(const Device::ProtocolFactoryList& pl, QWidget* parent);
  ~DeviceEditDialog();

  Device::DeviceSettings getSettings() const;

  void setSettings(const Device::DeviceSettings& settings);

  // This mode will display a warning to
  // the user if he has to edit the device again.
  void setEditingInvalidState(bool);

private:
  void selectedProtocolChanged();
  void initAvailableProtocols();

  const Device::ProtocolFactoryList& m_protocolList;
  std::unique_ptr<Device::DeviceEnumerator> m_enumerator{};

  QDialogButtonBox* m_buttonBox{};
  QTreeWidget* m_protocols{};
  QListWidget* m_devices{};
  QLabel* m_devicesLabel{};
  Device::ProtocolSettingsWidget* m_protocolWidget{};
  QFormLayout* m_layout{};
  QList<Device::DeviceSettings> m_previousSettings;
  int m_index{};

  bool m_invalidState{false};
  void selectedDeviceChanged();
};
}
