#pragma once
#include <QAbstractItemModel>
#include <Device/Address/AddressSettings.hpp>
#include <QComboBox>
#include <QStyledItemDelegate>
#include <State/Widgets/Values/TypeComboBox.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

namespace Explorer {
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
  public:
    AddressItemModel(QObject* parent);
    enum Rows { Name, Address, Value, Type, Min, Max, Values, Unit,
                Access, Bounding, Repetition, Description, Tags, Count
              };

    void setState(
        DeviceExplorerModel* model,
        Device::NodePath nodepath,
        const Device::FullAddressSettings& s);
    void clear();


  private:
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant valueColumnData(const State::Value& val, int role) const;
    QVariant data(const QModelIndex& index, int role) const override;


    QPointer<DeviceExplorerModel> m_model;
    Device::NodePath m_path;
    Device::FullAddressSettings m_settings;

};

class AddressItemDelegate final : public QStyledItemDelegate
{
  public:
    AddressItemDelegate(QObject* parent=0);
    ~AddressItemDelegate();

  private:
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
};

}
