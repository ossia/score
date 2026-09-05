#pragma once
#include <Device/Address/AddressSettings.hpp>

#include <ossia/network/value/value.hpp>

#include <QRect>
#include <QWidget>

#include <optional>

#include <score_plugin_deviceexplorer_export.h>

#include <verdigris>

class QMenu;
class QPainter;
class QStyleOptionViewItem;

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

  //! Whether the user did anything; an untouched editor writes nothing back.
  bool edited() const noexcept { return m_edited; }

  //! The editor acts rather than holds a value (an impulse button), so it
  //! commits as it is used and not when it closes.
  virtual bool commitsImmediately() const noexcept { return false; }

  //! The parameter's declared default, for the "Reset to default" action.
  void setDefaultValue(ossia::value v) { m_default = std::move(v); }
  const ossia::value& defaultValue() const noexcept { return m_default; }

  //! Puts the declared default back as the user's own edit; nothing if none.
  void resetToDefault();

  /**
   * @brief Gives the editor and every field in it the shared value actions.
   *
   * Copy, paste, "Edit as text" and "Reset to default", on the right-click
   * that already types a value into a graphics slider elsewhere in score. They
   * act on the value as a whole, which is what a multi-widget editor cannot
   * otherwise offer: a vec4 has four fields and no way to paste four numbers.
   *
   * Called by the factories below; a widget built by hand calls it itself,
   * once its fields exist.
   */
  void installValueMenu();

  //! The value as text, for Copy and the text form.
  virtual QString toText() const;

  //! That text read back; empty when it names no value of this editor type,
  //! in which case nothing is committed.
  virtual std::optional<ossia::value> fromText(const QString& text) const;

  //! Whether the value travels as text at all; an impulse does not.
  virtual bool hasTextForm() const noexcept { return true; }

  //! Whether the editor is already a text field, so "Edit as text" is moot.
  virtual bool isTextual() const noexcept { return false; }

protected:
  virtual ossia::value getImpl() const = 0;
  virtual void setImpl(ossia::value t) = 0;

  void markEdited()
  {
    if(!m_setting)
      m_edited = true;
  }

  //! A value from outside the fields (paste, text form, default), reported as
  //! the user's own edit.
  void applyExternal(const ossia::value& v);

public:
  void changed(ossia::value arg_1)
      E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, changed, arg_1)

  /**
   * @brief Focus left the editor entirely.
   *
   * Not what makes a cell editor commit -- eventFilter() hands Qt a FocusOut
   * for that, so that the commit and the close stay Qt's. This is for anything
   * else that wants to know, and it is emitted after the commit.
   */
  void editingFinished() E_SIGNAL(SCORE_PLUGIN_DEVICEEXPLORER_EXPORT, editingFinished)

private:
  bool eventFilter(QObject* obj, QEvent* ev) override;
  void contextMenuEvent(QContextMenuEvent* ev) override;
  void popValueMenu(QMenu* menu, QPoint globalPos);

  ossia::value m_default;
  bool m_setting{};
  bool m_edited{};
};

//! How much room the editor has; a compact one drops sliders and pads.
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

//! Puts an editor in a table cell and no further.
//!
//! A tree row is around 18px and a spin box asks for 26, so an editor left at
//! its size hint paints over its neighbours. The point size is stepped down
//! until the natural height fits, then the widget is squeezed.
SCORE_PLUGIN_DEVICEEXPLORER_EXPORT
void fitEditorToCell(QWidget& editor, const QRect& cell);

//! Draws a cell whose text carries a "[+N lines]" marker: the value in the
//! normal pen, the marker italic and dimmed. False when there is no marker and
//! the caller should paint the cell itself.
SCORE_PLUGIN_DEVICEEXPLORER_EXPORT
bool paintValueWithMarker(
    QPainter& painter, const QStyleOptionViewItem& option, const QString& text);
}
