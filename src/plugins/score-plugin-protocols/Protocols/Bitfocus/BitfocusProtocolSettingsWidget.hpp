#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>

#include <QJSEngine>
#include <QVariant>

#include <functional>
#include <optional>
#include <verdigris>

class QStackedLayout;
class QLineEdit;
class QVBoxLayout;
class QFormLayout;
class QSpinBox;
class QWidget;
class QLabel;
class QScrollArea;

namespace Protocols
{
class BitfocusProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit BitfocusProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

  using Device::ProtocolSettingsWidget::checkForChanges;

private:
  void resetFields();
  void updateFields();
  void resizeEvent(QResizeEvent*) override;
  BitfocusSpecificSettings m_settings;

  QFormLayout* m_rootLayout{};
  QLineEdit* m_deviceNameEdit{};
  QScrollArea* m_scroll{};
  QWidget* m_subWidget{};
  QVBoxLayout* m_subForm{};

  // Get the configuration for each widget
  struct widget
  {
    QLabel* label{};
    QWidget* widg{};
    std::function<ossia::value()> getValue;
    std::function<void(ossia::value)> setValue;

    // The value shown when loaded, and the one it came from
    ossia::value shown;
    std::optional<ossia::value> source;
  };

  void loadValue(const QString& id, std::optional<ossia::value> v);

  std::map<QString, widget> m_widgets;
  QMetaObject::Connection m_configurationParsed;
  QMetaObject::Connection m_configurationSaved;
  QJSEngine m_uiEngine;
  bool m_hasInitLabel{};
  bool m_fieldsLoaded{};
};
}
