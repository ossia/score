#include "NodeDisplayMethods.hpp"
#include <QFont>
#include <QBrush>
namespace DeviceExplorer
{
// TODO boost::visitor ?
QVariant nameColumnData(const iscore::Node& node, int role)
{
    using namespace iscore;
    if(node.is<DeviceSettings>())
    {
        if(role == Qt::DisplayRole || role == Qt::EditRole)
        {
            return node.get<DeviceSettings>().name;
        }
    }
    else
    {
        if(role == Qt::DisplayRole || role == Qt::EditRole)
        {
            return node.displayName();
        }
        else if(role == Qt::FontRole)
        {
            const IOType ioType = node.get<AddressSettings>().ioType;

            if(ioType == IOType::In || ioType == IOType::Out)
            {
                QFont f; // = QAbstractItemModel::data(index, role); //B: how to get current font ?
                f.setItalic(true);
                return f;
            }
        }
        else if(role == Qt::ForegroundRole)
        {
            const IOType ioType = node.get<AddressSettings>().ioType;

            if(ioType == IOType::In || ioType == IOType::Out)
            {
                return QBrush(Qt::black);
            }
        }
    }

    return {};
}

QVariant valueColumnData(const iscore::Node& node, int role)
{
    using namespace iscore;
    if(node.is<DeviceSettings>())
        return {};

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        const auto& val = node.get<AddressSettings>().value;
        if(val.val.is<iscore::tuple_t>())
        {
            // TODO a nice editor for tuples.
            return iscore::convert::toPrettyString(val);
        }
        else
        {
            return iscore::convert::value<QVariant>(val);
        }

    }
    else if(role == Qt::ForegroundRole)
    {
        const IOType ioType = node.get<AddressSettings>().ioType;

        if(ioType == IOType::In || ioType == IOType::Out)
        {
            return QBrush(Qt::black);
        }
    }

    return {};
}

QVariant GetColumnData(const iscore::Node& node, int role)
{
    using namespace iscore;
    if(node.is<DeviceSettings>())
        return {};

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(node.get<AddressSettings>().ioType)
        {
            case IOType::In:    return true;
            case IOType::Out:   return false;
            case IOType::InOut: return true;
            default:            return QVariant{};
        }
    }
    if(role == Qt::CheckStateRole)
    {
        switch(node.get<AddressSettings>().ioType)
        {
            case IOType::In:    return Qt::Checked;
            case IOType::Out:   return Qt::Unchecked;
            case IOType::InOut: return Qt::Checked;
            default:            return Qt::Unchecked;
        }
    }

    return {};
}
QVariant SetColumnData(const iscore::Node& node, int role)
{
    using namespace iscore;
    if(node.is<DeviceSettings>())
        return {};

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(node.get<AddressSettings>().ioType)
        {
            case IOType::In:    return false;
            case IOType::Out:   return true;
            case IOType::InOut: return true;
            default:            return QVariant{};
        }
    }
    if(role == Qt::CheckStateRole)
    {
        switch(node.get<AddressSettings>().ioType)
        {
            case IOType::In:    return Qt::Unchecked;
            case IOType::Out:   return Qt::Checked;
            case IOType::InOut: return Qt::Checked;
            default:            return Qt::Unchecked;
        }
    }
    return {};
}

QVariant minColumnData(const iscore::Node& node, int role)
{
    using namespace iscore;
    if(node.is<DeviceSettings>())
        return {};

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return iscore::convert::value<QVariant>(node.get<AddressSettings>().domain.min);
    }

    return {};
}

QVariant maxColumnData(const iscore::Node& node, int role)
{
    using namespace iscore;
    if(node.is<DeviceSettings>())
        return {};

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return iscore::convert::value<QVariant>(node.get<AddressSettings>().domain.max);
    }

    return {};
}
}
