#pragma once

#include <Device/Protocol/DeviceSettings.hpp>

#include <QDialog>
#include <QList>

#include <score_plugin_deviceexplorer_export.h>

#include <verdigris>

class QComboBox;
class QFormLayout;
class QWidget;

namespace Device
{
class ProtocolFactoryList;
class ProtocolSettingsWidget;
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

protected:
  void updateProtocolWidget();
  W_SLOT(updateProtocolWidget);

protected:
  void buildGUI();

  void initAvailableProtocols();

protected:
  const Device::ProtocolFactoryList& m_protocolList;

  QComboBox* m_protocolCBox;
  Device::ProtocolSettingsWidget* m_protocolWidget;
  QFormLayout* m_layout;
  QList<Device::DeviceSettings> m_previousSettings;
  int m_index;

  bool m_invalidState{false};
};
}
