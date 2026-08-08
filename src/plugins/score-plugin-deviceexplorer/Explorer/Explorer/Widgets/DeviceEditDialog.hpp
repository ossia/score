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
class DeviceCatalog;
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

  //! The tree listing the devices found by the protocol's enumerators.
  //! Selecting one of its items is what the user does to pick a camera,
  //! a joystick, etc.
  QTreeWidget* devicesTree() const noexcept { return m_devices; }

private:
  //! Null for a document whose score runs here: the dialogs then show this
  //! machine's protocols and hardware, as they always have.
  Device::DeviceCatalog* catalog() const noexcept;

  void selectedProtocolChanged();
  void selectedDeviceChanged();
  void selectedPresetChanged();
  QString editedDeviceName() const;
  void initAvailableProtocols();
  void initPresets();
  void clearEnumerators();
  void applyPreset(Device::Node n);

  //! The column listing what is plugged in. Shown when there is something in
  //! it: most protocols enumerate nothing.
  void showDevicesColumn();
  void hideDevicesColumn();

  const DeviceExplorerModel& m_model;
  const Device::ProtocolFactoryList& m_protocolList;
  Mode m_mode{};
  std::vector<std::pair<QString, std::unique_ptr<Device::DeviceEnumerator>>>
      m_enumerators;
  // Receiver of every enumerator connection of the current selection: it is
  // deleted before the QTreeWidgetItems those connections capture, which makes
  // Qt discard the queued metacalls still posted to it.
  QObject* m_enumeratorContext{};

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

  //! What was chosen from a preset or from the other machine's hardware. It is
  //! what getSettings() answers when this build has no widget for the protocol.
  Device::DeviceSettings m_chosenSettings{};

  QString m_originalName{};
  int m_index{};

  //! Which protocol the device list is currently showing, so that answers
  //! arriving for a previous one are dropped.
  UuidKey<Device::ProtocolFactory> m_currentProtocol{};
};
}
