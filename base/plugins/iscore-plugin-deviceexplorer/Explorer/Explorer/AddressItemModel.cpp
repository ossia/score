#include "AddressItemModel.hpp"
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/value/value_traits.hpp>
#include <ossia/network/base/node_attributes.hpp>
namespace Explorer
{

AddressItemModel::AddressItemModel(QObject* parent):
  QAbstractItemModel{parent}
{

}

void AddressItemModel::setAddress(const Device::FullAddressSettings& s)
{
  beginResetModel();
  m_settings = s;
  endResetModel();
}

void AddressItemModel::clear()
{
  beginResetModel();
  m_settings = {};
  endResetModel();
}

QModelIndex AddressItemModel::index(int row, int column, const QModelIndex& parent) const
{
  if(parent == QModelIndex{})
  {
    return createIndex(row, column, nullptr);
  }
  return {};
}

QModelIndex AddressItemModel::parent(const QModelIndex& child) const
{
  return {};
}

int AddressItemModel::rowCount(const QModelIndex& parent) const
{
  if(m_settings.address.device.isEmpty())
    return 0;

  if(!m_settings.value.valid())
    return 2;

  return Rows::Count + m_settings.extendedAttributes.size();
}

int AddressItemModel::columnCount(const QModelIndex& parent) const
{
  return 2;
}

QVariant AddressItemModel::valueColumnData(const State::Value& val, int role) const
{
  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    if (ossia::is_array(val))
    {
      // TODO a nice editor for lists.
      return State::convert::toPrettyString(val);
    }
    else
    {
      return State::convert::value<QVariant>(val);
    }
  }

  return {};
}

QVariant AddressItemModel::data(const QModelIndex& index, int role) const
{
  if(role != Qt::DisplayRole)
    return QVariant{};

  if(!m_settings.value.valid())
  {
    switch(index.column())
    {
      case 0:
      {
        switch(index.row())
        {
          case Rows::Name: return tr("Name");
          case Rows::Address: return tr("Address");
        }
      }
      case 1:
      {
        switch(index.row())
        {
          case Rows::Name: return m_settings.address.path.last();
          case Rows::Address: return m_settings.address.toString();
        }
      }
      default: break;
    }
    return {};
  }

  switch(index.column())
  {
    case 0:
    {
      switch(index.row())
      {
        case Rows::Name: return tr("Name");
        case Rows::Address: return tr("Address");
        case Rows::Value: return tr("Value");
        case Rows::Type: return tr("Type");
        case Rows::Unit: return tr("Unit");
        case Rows::Access: return tr("Access");
        case Rows::Bounding: return tr("Bounding");
        case Rows::Repetition: return tr("Repetition");
        case Rows::Description: return tr("Description");
        case Rows::Tags: return tr("Tags");
        default:
        {
          int idx = index.row() - Rows::Count;
          if(idx >= 0 && idx < m_settings.extendedAttributes.size())
          {
            auto it = m_settings.extendedAttributes.begin();
            std::advance(it, idx);
            return QString::fromStdString(it.key());
          }
        }
      }

      break;
    }
    case 1:
    {
      switch(index.row())
      {
        case Rows::Name: return m_settings.address.path.last();
        case Rows::Address: return m_settings.address.toString();
        case Rows::Value: return valueColumnData(m_settings.value, role);
        case Rows::Type: { return State::convert::ValuePrettyTypesArray()[(int)m_settings.value.getType()]; }
        case Rows::Unit: return QString::fromStdString(ossia::get_pretty_unit_text(m_settings.unit.get()));
        case Rows::Access: { return m_settings.ioType ? Device::AccessModeText()[*m_settings.ioType] : tr("None"); }
        case Rows::Bounding: { return Device::ClipModePrettyStringMap()[m_settings.clipMode]; }
        case Rows::Repetition: return (bool) m_settings.repetitionFilter;
        case Rows::Description: {
          auto desc = ossia::net::get_description(m_settings.extendedAttributes);
          if(desc)
            return QString::fromStdString(*desc);
          return {};
        }
        case Rows::Tags:
        {
          QStringList l;

          const auto& tags = ossia::net::get_tags(m_settings.extendedAttributes);
          if(tags)
            for(const auto& s : *tags)
              l += QString::fromStdString(s);

          return l;
        }
        default:
        {
          return {};
        }
      }

      break;
    }
    default:
      break;
  }

  return {};
}

}
