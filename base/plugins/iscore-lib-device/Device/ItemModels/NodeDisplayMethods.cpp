#include "NodeDisplayMethods.hpp"
#include <Device/Protocol/DeviceInterface.hpp>
#include <QFont>
#include <QBrush>
namespace DeviceExplorer
{
// TODO boost::visitor ?

QVariant nameColumnData(const iscore::Node& node, int role)
{
    static const QFont italicFont{[] () { QFont f; f.setItalic(true); return f; }()};

    using namespace iscore;

    const IOType ioType = node.get<AddressSettings>().ioType;
    switch(role)
    {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return node.displayName();
        case Qt::FontRole:
        {
            if(ioType == IOType::In || ioType == IOType::Out)
            {
                return italicFont;
            }
        }
        case Qt::ForegroundRole:
        {
            if(ioType == IOType::In || ioType == IOType::Out)
            {
                return QBrush(Qt::lightGray);
            }
        }
        default:
            return {};
    }
}

QVariant deviceNameColumnData(const iscore::Node& node, DeviceInterface& dev, int role)
{
    static const QFont italicFont{[] () { QFont f; f.setItalic(true); return f; }()};

    using namespace iscore;

    switch(role)
    {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return node.get<DeviceSettings>().name;
        case Qt::FontRole:
        {
            if(!dev.connected())
                return italicFont;
        }
        default:
            return {};
    }
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
            case IOType::Invalid: return QVariant{};
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
            case IOType::Invalid: return Qt::Unchecked;
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
            case IOType::Invalid: return true;
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
            case IOType::Invalid: return Qt::Unchecked;
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
