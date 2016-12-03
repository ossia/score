#include <Device/Protocol/DeviceInterface.hpp>
#include <QBrush>
#include <QFont>
#include <qnamespace.h>

#include "NodeDisplayMethods.hpp"
#include <ossia/editor/value/value_conversion.hpp>
#include <ossia/network/domain/domain.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

namespace ossia
{

// TODO MOVEME
template <>
QVariant convert(const ossia::value& val)
{
  struct vis
  {
  public:
    using return_type = QVariant;
    return_type operator()(const ossia::Impulse& v) const
    {
      return QVariant::fromValue(v);
    }
    return_type operator()(int i) const
    {
      return QVariant::fromValue(i);
    }
    return_type operator()(float f) const
    {
      return QVariant::fromValue(f);
    }
    return_type operator()(bool b) const
    {
      return QVariant::fromValue(b);
    }
    return_type operator()(const std::string& s) const
    {
      return QString::fromStdString(s);
    }
    return_type operator()(char c) const
    {
      return QVariant::fromValue(QChar(c));
    }

    return_type operator()(ossia::Vec2f t) const
    {
      return QVector2D{t[0], t[1]};
    }
    return_type operator()(ossia::Vec3f t) const
    {
      return QVector3D{t[0], t[1], t[2]};
    }
    return_type operator()(ossia::Vec4f t) const
    {
      return QVector4D{t[0], t[1], t[2], t[3]};
    }
    return_type operator()(const std::vector<ossia::value>& t) const
    {
      QVariantList arr;
      arr.reserve(t.size());

      for (const auto& elt : t)
      {
        arr.push_back(eggs::variants::apply(*this, elt.v));
      }

      return arr;
    }

    return_type operator()(const ossia::Destination& t) const
    {
      return {};
    }
    return_type operator()() const
    {
      return {};
    }
  };

  return val.apply(vis{});
}
}
namespace Device
{
// TODO boost::visitor ?

QVariant nameColumnData(const Device::Node& node, int role)
{
  static const QFont italicFont{[]() {
    QFont f;
    f.setItalic(true);
    return f;
  }()};

  using namespace iscore;

  const Device::IOType ioType = node.get<Device::AddressSettings>().ioType;
  switch (role)
  {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return node.displayName();
    case Qt::FontRole:
    {
      if (ioType == IOType::In || ioType == IOType::Out)
      {
        return italicFont;
      }
    }
    case Qt::ForegroundRole:
    {
      if (ioType == IOType::In || ioType == IOType::Out)
      {
        return QBrush(Qt::lightGray);
      }
    }
    default:
      return {};
  }
}

QVariant
deviceNameColumnData(const Device::Node& node, DeviceInterface& dev, int role)
{
  static const QFont italicFont{[]() {
    QFont f;
    f.setItalic(true);
    return f;
  }()};

  using namespace iscore;

  switch (role)
  {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return node.get<DeviceSettings>().name;
    case Qt::FontRole:
    {
      if (!dev.connected())
        return italicFont;
    }
    default:
      return {};
  }
}

QVariant valueColumnData(const Device::Node& node, int role)
{
  using namespace iscore;
  if (node.is<DeviceSettings>())
    return {};

  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    const State::Value& val = node.get<AddressSettings>().value;
    if (val.val.isArray())
    {
      // TODO a nice editor for tuples.
      return State::convert::toPrettyString(val);
    }
    else
    {
      return State::convert::value<QVariant>(val);
    }
  }
  else if (role == Qt::ForegroundRole)
  {
    const IOType ioType = node.get<AddressSettings>().ioType;

    switch (ioType)
    {
      case IOType::In:
        return QBrush(Qt::darkGray);
      case IOType::Out:
        return QBrush(Qt::lightGray);
      default:
        return {};
    }
  }

  return {};
}

QVariant GetColumnData(const Device::Node& node, int role)
{
  using namespace iscore;
  if (node.is<DeviceSettings>())
    return {};

  /*
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
  */

  if (role == Qt::CheckStateRole)
  {
    switch (node.get<AddressSettings>().ioType)
    {
      case IOType::In:
        return Qt::Checked;
      case IOType::Out:
        return Qt::Unchecked;
      case IOType::InOut:
        return Qt::Checked;
      case IOType::Invalid:
        return Qt::Unchecked;
      default:
        return Qt::Unchecked;
    }
  }

  return {};
}
QVariant SetColumnData(const Device::Node& node, int role)
{
  using namespace iscore;
  if (node.is<DeviceSettings>())
    return {};

  /*
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
  */

  if (role == Qt::CheckStateRole)
  {
    switch (node.get<AddressSettings>().ioType)
    {
      case IOType::In:
        return Qt::Unchecked;
      case IOType::Out:
        return Qt::Checked;
      case IOType::InOut:
        return Qt::Checked;
      case IOType::Invalid:
        return Qt::Unchecked;
      default:
        return Qt::Unchecked;
    }
  }
  return {};
}

QVariant minColumnData(const Device::Node& node, int role)
{
  using namespace iscore;
  if (node.is<DeviceSettings>())
    return {};

  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    return node.get<AddressSettings>().domain.get().convert_min<QVariant>();
  }

  return {};
}

QVariant maxColumnData(const Device::Node& node, int role)
{
  using namespace iscore;
  if (node.is<DeviceSettings>())
    return {};

  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    return node.get<AddressSettings>().domain.get().convert_max<QVariant>();
  }

  return {};
}
}
