#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <State/Widgets/Values/TypeComboBox.hpp>

#include <QAbstractItemModel>
#include <QStyledItemDelegate>

#include <verdigris>

namespace Explorer
{
class DeviceExplorerModel;
/**
 * @brief Displays an address
 *
 * Rows:
 * * Name
 * * Full address
 * * Value
 * * Unit
 * * Type
 * * Domain
 * * Access mode
 * * Bounding mode
 * * Repetition filter
 * * Description
 * * Tags
 * * Extended attributes
 */
class AddressItemModel final : public QAbstractItemModel
{
  W_OBJECT(AddressItemModel)
public:
  AddressItemModel(QObject* parent);
  enum Rows
  {
    Name,
    Address,
    Value,
    Type,
    Min,
    Max,
    Values,
    Unit,
    Access,
    Bounding,
    Repetition,
    Count
  };

  void setState(
      DeviceExplorerModel* model,
      Device::NodePath nodepath,
      const Device::FullAddressSettings& s);
  void clear();

  const Device::FullAddressSettings& settings() const { return m_settings; }

  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant valueColumnData(const State::Value& val, int role) const;
  QVariant data(const QModelIndex& index, int role) const override;

private:
  int extendedCount() const noexcept;
  QPointer<DeviceExplorerModel> m_model;
  Device::NodePath m_path;
  Device::FullAddressSettings m_settings;
};

class AddressItemDelegate final : public QStyledItemDelegate
{
public:
  AddressItemDelegate(QObject* parent = 0);
  ~AddressItemDelegate();

private:
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
      const override;
  QWidget* createEditor(
      QWidget* parent,
      const QStyleOptionViewItem& option,
      const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index)
      const override;
};

class AddressValueWidget : public QWidget
{
  W_OBJECT(AddressValueWidget)
public:
  using QWidget::QWidget;
  virtual ossia::value get() const = 0;
  virtual void set(ossia::value t) = 0;

public:
  void changed(ossia::value arg_1) W_SIGNAL(changed, arg_1);
};
}
