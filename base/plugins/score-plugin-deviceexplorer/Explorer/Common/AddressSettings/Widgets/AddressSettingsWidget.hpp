#pragma once

#include <Device/Address/AddressSettings.hpp>
#include <QLabel>
#include <QWidget>
#include <score/widgets/TextLabel.hpp>
#include <score_plugin_deviceexplorer_export.h>
#include <QComboBox>
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
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddressSettingsWidget
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
  virtual void setCanEditProperties(bool) = 0;

protected:
  Device::AddressSettings getCommonSettings() const;
  void setCommonSettings(const Device::AddressSettings&);

  QFormLayout* m_layout;

private:
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
    Q_OBJECT
  public:
    AccessModeComboBox(QWidget* parent);
    virtual ~AccessModeComboBox();

    ossia::access_mode get() const;
    void set(ossia::access_mode t);

  Q_SIGNALS:
    void changed(ossia::access_mode);
};

class BoundingModeComboBox final : public QComboBox
{
    Q_OBJECT
  public:
    BoundingModeComboBox(QWidget* parent);
    virtual ~BoundingModeComboBox();

    ossia::bounding_mode get() const;
    void set(ossia::bounding_mode t);

  Q_SIGNALS:
    void changed(ossia::bounding_mode);
};

inline QLabel* makeLabel(QString text, QWidget* parent)
{
  auto label = new TextLabel{std::move(text), parent};
  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  return label;
}
}
