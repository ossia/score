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

// TODO rework this to use iscore::Value::toString();
QVariant valueColumnData(const iscore::Node& node, int role)
{
    using namespace iscore;
    if(node.is<DeviceSettings>())
        return {};

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        return iscore::convert::toPrettyString(node.get<AddressSettings>().value);
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

QVariant IOTypeColumnData(const iscore::Node& node, int role)
{
    using namespace iscore;
    if(node.is<DeviceSettings>())
        return {};

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(node.get<AddressSettings>().ioType)
        {
            case IOType::In:
                return QString("<-");

            case IOType::Out:
                return QString("->");

            case IOType::InOut:
                return QString("<->");

            default:
                return {};
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
        return node.get<AddressSettings>().domain.min.toQVariant();
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
        return node.get<AddressSettings>().domain.max.toQVariant();
    }

    return {};
}
}
