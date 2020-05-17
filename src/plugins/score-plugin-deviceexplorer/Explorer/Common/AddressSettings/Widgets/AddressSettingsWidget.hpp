#pragma once

#include <Device/Address/AddressSettings.hpp>

#include <score/widgets/TextLabel.hpp>

#include <QComboBox>
#include <QLabel>
#include <QWidget>

#include <score_plugin_deviceexplorer_export.h>

#include <verdigris>
class QComboBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QFormLayout;

namespace State
{
class UnitWidget;
}
namespace Explorer
{
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddressSettingsWidget : public QWidget
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
  virtual void setCanEditProperties(bool) = 0;

protected:
  Device::AddressSettings getCommonSettings() const;
  void setCommonSettings(const Device::AddressSettings&);

  QFormLayout* m_layout{};

  QComboBox* m_ioTypeCBox{};
  QComboBox* m_clipModeCBox{};
  QCheckBox* m_repetition{};
  QComboBox* m_tagsEdit{};
  QPushButton* m_addTagButton{};
  QLineEdit* m_description{};
  State::UnitWidget* m_unit{};
  bool m_none_type{false};
};

class AccessModeComboBox final : public QComboBox
{
  W_OBJECT(AccessModeComboBox)
public:
  AccessModeComboBox(QWidget* parent);
  virtual ~AccessModeComboBox();

  ossia::access_mode get() const;
  void set(ossia::access_mode t);

public:
  void changed(ossia::access_mode arg_1)
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, changed, arg_1);
};

class BoundingModeComboBox final : public QComboBox
{
  W_OBJECT(BoundingModeComboBox)
public:
  BoundingModeComboBox(QWidget* parent);
  virtual ~BoundingModeComboBox();

  ossia::bounding_mode get() const;
  void set(ossia::bounding_mode t);

public:
  void changed(ossia::bounding_mode arg_1)
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, changed, arg_1);
};

inline QLabel* makeLabel(QString text, QWidget* parent)
{
  auto label = new TextLabel{std::move(text), parent};
  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  return label;
}
}
