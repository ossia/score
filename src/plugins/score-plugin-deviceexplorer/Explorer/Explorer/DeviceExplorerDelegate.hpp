#pragma once
#include <QPointer>
#include <QTimer>
#include <QSet>
#include <QStyledItemDelegate>

#include <score_plugin_deviceexplorer_export.h>

namespace Device
{
struct AddressSettings;
}

namespace Explorer
{
//! Per-type editors for the Value column of the device explorer tree.
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceExplorerDelegate final
    : public QStyledItemDelegate
{
public:
  explicit DeviceExplorerDelegate(QObject* parent = nullptr);
  ~DeviceExplorerDelegate();

  //! The settings of the address at that index, or null.
  static QModelIndex sourceIndex(const QModelIndex& index) noexcept;
  static const Device::AddressSettings* addressAt(const QModelIndex& index) noexcept;

private:
  QWidget* createEditor(
      QWidget* parent, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index)
      const override;

  //! Called by QAbstractItemView once the editor has left the view's map, so
  //! owns() is exact rather than a guess at the widget's state.
  void destroyEditor(QWidget* editor, const QModelIndex& index) const override;

  //! The editors this delegate made that the view still owns.
  bool owns(QWidget* editor) const noexcept { return m_live.contains(editor); }
  void paint(
      QPainter* painter, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override;

  //! An impulse is a button in the row, pressable on the first click.
  bool editorEvent(
      QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
      const QModelIndex& index) override;

  //! Lights an impulse row for a moment, in both directions: one the user
  //! sends, and one arriving from the device.
  void flash(const QModelIndex& index);

  //! Follows whichever model the view puts in front of us, so a value arriving
  //! from the device lights its row.
  void watch(const QAbstractItemModel* model) const;

  mutable QSet<QWidget*> m_live;
  QPersistentModelIndex m_pressed;
  mutable QList<QPersistentModelIndex> m_lit;
  mutable QPointer<QTimer> m_unlit;
  mutable QPointer<const QAbstractItemModel> m_seen;
  mutable QPointer<const QAbstractItemModel> m_watched;
  mutable QMetaObject::Connection m_traffic;
  mutable QPointer<QWidget> m_surface;
  void updateEditorGeometry(
      QWidget* editor, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override;
};
}
