#pragma once

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <QDialog>
#include <QList>
#include <QSplitter>

#include <score_plugin_deviceexplorer_export.h>

#include <verdigris>

class QComboBox;
class QFormLayout;
class QWidget;
class QListWidget;
class QTreeWidget;
class QStackedWidget;
class QVBoxLayout;
class QLabel;
class QDialogButtonBox;
class QPushButton;

namespace Device
{
class ProtocolFactoryList;
class ProtocolSettingsWidget;
class DeviceEnumerator;
}
namespace Explorer
{
class DeviceExplorerModel;
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceEditDialog final : public QDialog
{
  W_OBJECT(DeviceEditDialog)

public:
  enum Mode
  {
    Creating,
    Editing
  };
  explicit DeviceEditDialog(
      const DeviceExplorerModel& model, const Device::ProtocolFactoryList& pl, Mode mode,
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
  void selectedPresetChanged();
  void initAvailableProtocols();
  void initPresets();

  const DeviceExplorerModel& m_model;
  const Device::ProtocolFactoryList& m_protocolList;
  Mode m_mode{};
  std::vector<std::pair<QString, std::unique_ptr<Device::DeviceEnumerator>>>
      m_enumerators;

  QSplitter* m_splitter{};
  QDialogButtonBox* m_buttonBox{};
  QPushButton* m_okButton{};
  QPushButton* m_helpButton{};

  // Column 1: tab buttons + stacked protocols/presets
  QPushButton* m_protocolsTabButton{};
  QPushButton* m_presetsTabButton{};
  QStackedWidget* m_column1Stack{};
  QTreeWidget* m_protocols{};
  QTreeWidget* m_presets{};

  QTreeWidget* m_devices{};
  // QWidget* m_main{};
  QLabel* m_devicesLabel{};
  Device::ProtocolSettingsWidget* m_protocolWidget{};
  // QFormLayout* m_settingsFormLayout{};
  QVBoxLayout* m_column3Layout{};
  QList<Device::DeviceSettings> m_previousSettings;
  QLabel* m_invalidLabel{};
  QLabel* m_protocolNameLabel{};

  // For presets: the loaded node with full address tree
  Device::Node m_presetNode{};

  QString m_originalName{};
  int m_index{};
};
}
