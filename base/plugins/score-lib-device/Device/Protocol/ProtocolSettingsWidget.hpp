#pragma once

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <QString>
#include <QDialog>
#include <QWidget>
#include <score_lib_device_export.h>

namespace Device
{
class SCORE_LIB_DEVICE_EXPORT ProtocolSettingsWidget : public QWidget
{
public:
  explicit ProtocolSettingsWidget(QWidget* parent = nullptr) : QWidget(parent)
  {
  }

  virtual ~ProtocolSettingsWidget();
  virtual Device::DeviceSettings getSettings() const = 0;
  virtual QString getPath() const // TODO berk, remove me
  {
    return QString("");
  }
  virtual void setSettings(const Device::DeviceSettings& settings) = 0;
};

class SCORE_LIB_DEVICE_EXPORT AddAddressDialog : public QDialog
{
  public:
    using QDialog::QDialog;
    virtual ~AddAddressDialog();
    virtual Device::AddressSettings getSettings() const = 0;
};
}
