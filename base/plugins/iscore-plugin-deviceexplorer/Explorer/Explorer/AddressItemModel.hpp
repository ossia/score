#pragma once
#include <QAbstractItemModel>
#include <Device/Address/AddressSettings.hpp>

namespace Explorer {
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
class AddressItemModel : public QAbstractItemModel
{
  public:
    AddressItemModel(QObject* parent);

    void setAddress(const Device::FullAddressSettings& s);
    void clear();

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;

    QVariant
    valueColumnData(const State::Value& val, int role) const;

    enum Rows { Name, Address, Value, Type, Min, Max, Values, Unit,
                Access, Bounding, Repetition, Description, Tags, Count
              };
    QVariant data(const QModelIndex& index, int role) const override;
  private:
    Device::FullAddressSettings m_settings;
};
}
