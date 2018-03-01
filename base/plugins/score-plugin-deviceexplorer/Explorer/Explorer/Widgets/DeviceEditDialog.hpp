#pragma once

#include <QDialog>
#include <QList>
#include <QString>

#include <Device/Protocol/DeviceSettings.hpp>

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
class DeviceEditDialog final : public QDialog
{
  Q_OBJECT

public:
  explicit DeviceEditDialog(
      const Device::ProtocolFactoryList& pl, QWidget* parent);
  ~DeviceEditDialog();

  Device::DeviceSettings getSettings() const;
  QString getPath() const;

  void setSettings(const Device::DeviceSettings& settings);

  // This mode will display a warning to
  // the user if he has to edit the device again.
  void setEditingInvalidState(bool);

protected Q_SLOTS:

  void updateProtocolWidget();

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
