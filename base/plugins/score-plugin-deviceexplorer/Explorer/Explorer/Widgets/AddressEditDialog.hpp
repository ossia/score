#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include <QDialog>
#include <score/widgets/WidgetWrapper.hpp>

class QComboBox;
class QFormLayout;
class QLineEdit;
class QWidget;

namespace Explorer
{

class AddressSettingsWidget;
class AddressEditDialog final : public Device::AddAddressDialog
{
  Q_OBJECT

public:
  // Creation of an address
  explicit AddressEditDialog(QWidget* parent);

  // Edition of an address
  explicit AddressEditDialog(
      const Device::AddressSettings& addr, QWidget* parent);
  ~AddressEditDialog();

  Device::AddressSettings getSettings() const override;
  static Device::AddressSettings makeDefaultSettings();

  void setCanRename(bool);
  void setCanEditProperties(bool);

protected:
  void setNodeSettings();
  void setValueSettings();
  void updateType(ossia::val_type valueType);

  Device::AddressSettings m_originalSettings;
  QLineEdit* m_nameEdit{};
  QComboBox* m_valueTypeCBox{};
  WidgetWrapper<AddressSettingsWidget>* m_addressWidget{};
  QFormLayout* m_layout{};
  bool m_canEdit{true};
};

}
