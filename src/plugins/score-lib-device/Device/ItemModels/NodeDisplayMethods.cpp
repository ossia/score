// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "NodeDisplayMethods.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <State/Domain.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QBrush>
#include <QFont>
#include <qnamespace.h>

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
    return_type operator()(const ossia::impulse& v) const { return QVariant::fromValue(v); }
    return_type operator()(int i) const { return QVariant::fromValue(i); }
    return_type operator()(float f) const { return QVariant::fromValue(f); }
    return_type operator()(bool b) const { return QVariant::fromValue(b); }
    return_type operator()(const std::string& s) const { return QString::fromStdString(s); }
    return_type operator()(char c) const { return QVariant::fromValue(QChar(c)); }

    return_type operator()(ossia::vec2f t) const { return QVector2D{t[0], t[1]}; }
    return_type operator()(ossia::vec3f t) const { return QVector3D{t[0], t[1], t[2]}; }
    return_type operator()(ossia::vec4f t) const { return QVector4D{t[0], t[1], t[2], t[3]}; }
    return_type operator()(const std::vector<ossia::value>& t) const
    {
      QVariantList arr;
      arr.reserve(t.size());

      for (const auto& elt : t)
      {
        arr.push_back(ossia::apply_nonnull(*this, elt.v));
      }

      return arr;
    }
    return_type operator()() const { return {}; }
  };

  return val.apply(vis{});
}
}
namespace Device
{
QVariant nameColumnData(const Device::Node& node, int role)
{
  static const QFont& italicFont{[]() {
    static QFont f;
    f.setItalic(true);
    return f;
  }()};

  using namespace score;

  const auto ioType = node.get<Device::AddressSettings>().ioType;
  switch (role)
  {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return node.displayName();
    case Qt::FontRole:
    {
      if (ioType == ossia::access_mode::GET || ioType == ossia::access_mode::SET)
      {
        return italicFont;
      }
      return {};
    }
    case Qt::ForegroundRole:
    {
      if (ioType == ossia::access_mode::GET || ioType == ossia::access_mode::SET)
      {
        return QBrush(Qt::lightGray);
      }
      return {};
    }
    default:
      return {};
  }
}

QVariant deviceNameColumnData(const Device::Node& node, bool connected, int role)
{
  static const QFont& italicFont{[]() {
    static QFont f;
    f.setItalic(true);
    return f;
  }()};

  using namespace score;

  switch (role)
  {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return node.get<DeviceSettings>().name;
    case Qt::FontRole:
    {
      if (!connected)
        return italicFont;
      return {};
    }
    default:
      return {};
  }
}

QVariant valueColumnData(const Device::Node& node, int role)
{
  using namespace score;
  if (node.is<DeviceSettings>())
    return {};

  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    const ossia::value& val = node.get<AddressSettings>().value;
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
  else if (role == Qt::ForegroundRole)
  {
    const auto ioType = node.get<AddressSettings>().ioType;

    if (ioType)
    {
      switch (*ioType)
      {
        case ossia::access_mode::GET:
          return QBrush(Qt::darkGray);
        case ossia::access_mode::SET:
          return QBrush(Qt::lightGray);
        default:
          return {};
      }
    }
  }

  return {};
}

QVariant GetColumnData(const Device::Node& node, int role)
{
  using namespace score;
  if (node.is<DeviceSettings>())
    return {};

  /*
  if(role == Qt::DisplayRole || role == Qt::EditRole)
  {
      switch(node.get<AddressSettings>().ioType)
      {
          case ossia::access_mode::GET:    return true;
          case ossia::access_mode::SET:   return false;
          case ossia::access_mode::BI: return true;
          case IOType::Invalid: return QVariant{};
          default:            return QVariant{};
      }
  }
  */

  if (role == Qt::CheckStateRole)
  {
    const auto ioType = node.get<AddressSettings>().ioType;
    if (ioType)
    {
      switch (*ioType)
      {
        case ossia::access_mode::GET:
          return Qt::Checked;
        case ossia::access_mode::SET:
          return Qt::Unchecked;
        case ossia::access_mode::BI:
          return Qt::Checked;
      }
    }
    return Qt::Unchecked;
  }

  return {};
}
QVariant SetColumnData(const Device::Node& node, int role)
{
  using namespace score;
  if (node.is<DeviceSettings>())
    return {};

  /*
  if(role == Qt::DisplayRole || role == Qt::EditRole)
  {
      switch(node.get<AddressSettings>().ioType)
      {
          case ossia::access_mode::GET:    return false;
          case ossia::access_mode::SET:   return true;
          case ossia::access_mode::BI: return true;
          case IOType::Invalid: return true;
          default:            return QVariant{};
      }
  }
  */

  if (role == Qt::CheckStateRole)
  {
    const auto ioType = node.get<AddressSettings>().ioType;
    if (ioType)
    {
      switch (*ioType)
      {
        case ossia::access_mode::GET:
          return Qt::Unchecked;
        case ossia::access_mode::SET:
          return Qt::Checked;
        case ossia::access_mode::BI:
          return Qt::Checked;
      }
    }
    return Qt::Unchecked;
  }
  return {};
}

QVariant minColumnData(const Device::Node& node, int role)
{
  using namespace score;
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
  using namespace score;
  if (node.is<DeviceSettings>())
    return {};

  if (role == Qt::DisplayRole || role == Qt::EditRole)
  {
    return node.get<AddressSettings>().domain.get().convert_max<QVariant>();
  }

  return {};
}
}
