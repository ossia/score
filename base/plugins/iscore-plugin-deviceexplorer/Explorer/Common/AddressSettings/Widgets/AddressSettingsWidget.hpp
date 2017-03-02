#pragma once

#include <Device/Address/AddressSettings.hpp>
#include <QLabel>
#include <QWidget>
#include <iscore/widgets/TextLabel.hpp>
#include <iscore_plugin_deviceexplorer_export.h>

class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QFormLayout;

namespace State
{
class UnitWidget;
}
namespace Explorer
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddressSettingsWidget
    : public QWidget
{
public:
  struct no_widgets_t
  {
  };
  explicit AddressSettingsWidget(QWidget* parent = nullptr);
  explicit AddressSettingsWidget(no_widgets_t, QWidget* parent = nullptr);

  virtual ~AddressSettingsWidget();

  virtual Device::AddressSettings getSettings() const = 0;
  virtual Device::AddressSettings getDefaultSettings() const = 0;
  virtual void setSettings(const Device::AddressSettings& settings) = 0;

protected:
  Device::AddressSettings getCommonSettings() const;
  void setCommonSettings(const Device::AddressSettings&);

  QFormLayout* m_layout;

private:
  bool m_none_type{false};
  QComboBox* m_ioTypeCBox{};
  QComboBox* m_clipModeCBox{};
  QCheckBox* m_repetition{};
  QComboBox* m_tagsEdit{};
  QLineEdit* m_description{};
  State::UnitWidget* m_unit{};
};

inline QLabel* makeLabel(QString text, QWidget* parent)
{
  auto label = new TextLabel{std::move(text), parent};
  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  return label;
}
}
