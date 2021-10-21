#pragma once

#include <Device/Node/DeviceNode.hpp>
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
class DeviceExplorerModel;
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceEditDialog final
    : public QDialog
{
  W_OBJECT(DeviceEditDialog)

public:
  enum Mode
  {
    Creating,
    Editing
  };
  explicit DeviceEditDialog(
      const DeviceExplorerModel& model,
      const Device::ProtocolFactoryList& pl,
      Mode mode,
      QWidget* parent);
  ~DeviceEditDialog();

  Device::DeviceSettings getSettings() const;
  Device::Node getDevice() const;

  void setSettings(const Device::DeviceSettings& settings);

  // This mode will display a warning to
  // the user if he has to edit the device again.
  void setAcceptEnabled(bool);

  // enable protocol & device browsing
  void setBrowserEnabled(bool);

  void updateValidity();

private:
  void selectedProtocolChanged();
  void selectedDeviceChanged();
  void initAvailableProtocols();

  const DeviceExplorerModel& m_model;
  const Device::ProtocolFactoryList& m_protocolList;
  Mode m_mode{};
  std::unique_ptr<Device::DeviceEnumerator> m_enumerator{};

  QDialogButtonBox* m_buttonBox{};
  QPushButton* m_okButton{};
  QTreeWidget* m_protocols{};
  QListWidget* m_devices{};
  QWidget* m_main{};
  QLabel* m_protocolsLabel{};
  QLabel* m_devicesLabel{};
  Device::ProtocolSettingsWidget* m_protocolWidget{};
  QFormLayout* m_layout{};
  QList<Device::DeviceSettings> m_previousSettings;
  QLabel* m_invalidLabel{};
  QLabel* m_protocolNameLabel{};

  QString m_originalName{};
  int m_index{};
};
}
