#pragma once
#include <Device/Address/AddressSettings.hpp>

#include <ossia/network/value/value.hpp>

#include <QWidget>

#include <score_plugin_deviceexplorer_export.h>

#include <verdigris>

namespace Explorer
{
//! An editor for one parameter value, shared by the tree, the address panel
//! and the add / edit address dialog.
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddressValueWidget : public QWidget
{
  W_OBJECT(AddressValueWidget)
public:
  using QWidget::QWidget;

  ossia::value get() const { return getImpl(); }

  //! Show a value without counting as an edit of it.
  void set(ossia::value t)
  {
    m_setting = true;
    setImpl(std::move(t));
    m_setting = false;
  }

  //! Whether the user did anything: an editor that was only opened and closed
  //! must not write back what it was showing.
  bool edited() const noexcept { return m_edited; }

  //! Whether the editor acts instead of holding a value, as an impulse button
  //! does, and so commits as it is used rather than when it closes.
  virtual bool commitsImmediately() const noexcept { return false; }

protected:
  virtual ossia::value getImpl() const = 0;
  virtual void setImpl(ossia::value t) = 0;

  void markEdited()
  {
    if(!m_setting)
      m_edited = true;
  }

public:
  void changed(ossia::value arg_1)
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, changed, arg_1)

private:
  bool m_setting{};
  bool m_edited{};
};

//! How much room the editor has: a compact one gives up the sliders and the
//! pads and keeps the fields.
enum class ValueEditorSize
{
  Compact,
  Full
};

//! The editor for a value, from its unit, its domain and its type.
SCORE_PLUGIN_DEVICEEXPLORER_EXPORT
AddressValueWidget* make_value_widget(
    const Device::AddressSettingsCommon& addr, QWidget* parent,
    ValueEditorSize size = ValueEditorSize::Full);

//! The editor for one bound of the domain.
SCORE_PLUGIN_DEVICEEXPLORER_EXPORT
AddressValueWidget*
make_bound_widget(const Device::AddressSettingsCommon& addr, QWidget* parent);

//! The editor for the list of accepted values.
SCORE_PLUGIN_DEVICEEXPLORER_EXPORT
AddressValueWidget*
make_values_widget(const Device::AddressSettingsCommon& addr, QWidget* parent);

//! Whether the parameter enumerates the values it accepts.
SCORE_PLUGIN_DEVICEEXPLORER_EXPORT
bool hasValueList(const Device::AddressSettingsCommon& addr) noexcept;
}
