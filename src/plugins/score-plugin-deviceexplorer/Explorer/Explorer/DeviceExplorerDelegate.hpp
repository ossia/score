#pragma once
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
  static const Device::AddressSettings* addressAt(const QModelIndex& index) noexcept;

private:
  QWidget* createEditor(
      QWidget* parent, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index)
      const override;
  void updateEditorGeometry(
      QWidget* editor, const QStyleOptionViewItem& option,
      const QModelIndex& index) const override;
};
}
