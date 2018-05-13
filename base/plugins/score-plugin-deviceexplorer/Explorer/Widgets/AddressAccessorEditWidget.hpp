#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <wobjectdefs.h>
#include <QWidget>
#include <score_plugin_deviceexplorer_export.h>

class QLineEdit;
namespace Explorer
{
class DeviceExplorerModel;
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddressAccessorEditWidget final
    : public QWidget
{
  W_OBJECT(AddressAccessorEditWidget)
public:
  AddressAccessorEditWidget(DeviceExplorerModel& model, QWidget* parent);

  void setAddress(const State::AddressAccessor& addr);
  void setFullAddress(Device::FullAddressAccessorSettings&& addr);

  const Device::FullAddressAccessorSettings& address() const;

  QString addressString() const;

  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent*) override;
public:
  void addressChanged(const Device::FullAddressAccessorSettings& arg_1) W_SIGNAL(addressChanged, arg_1);

private:
  void customContextMenuEvent(const QPoint& p);

  QLineEdit* m_lineEdit{};
  Device::FullAddressAccessorSettings m_address;
  DeviceExplorerModel& m_model;
};
}
