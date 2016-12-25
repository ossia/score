#include "JSONDeviceLoader.hpp"
#include <QFile>
#include <QJsonDocument>
#include <ossia/network/domain/domain.hpp>

namespace std {
template<>
struct hash<QString>
{
public:
  auto operator()(const QString& s) const
  {
    return qHash(s);
  }
};
}

namespace Device
{
using json_actions_t = iscore::hash_map<
QString,
std::function<
void(
Device::Node& node,
const QJsonValue& val)
>
>;

static State::Value fromTextualType(const QString& str)
{
  static const iscore::hash_map<QString, State::Value> value_map{
    {"boolean", State::Value::fromValue(false)},
    {"integer", State::Value::fromValue(0)},
    {"decimal", State::Value::fromValue(0.)},
    {"filepath", State::Value::fromValue("")},
    {"decimalArray", State::Value::fromValue(State::tuple_t{})},
    {"string", State::Value::fromValue("")}
  };
  auto it = value_map.find(str);
  if(it != value_map.end())
  {
    return it.value();
  }
  return {};
}

static ossia::net::domain fromJsonDomain(
    const QString& str,
    State::ValueType t)
{
  if(!str.isEmpty())
  {
    auto dom = str.split(' ');
    if(dom.size() == 2)
    {
      switch(t) {
        case State::ValueType::Int:
          return ossia::net::make_domain(dom[0].toInt(), dom[1].toInt());
          break;
        case State::ValueType::Float:
        case State::ValueType::Tuple:
        case State::ValueType::Vec2f:
        case State::ValueType::Vec3f:
        case State::ValueType::Vec4f:
          return ossia::net::make_domain(dom[0].toFloat(), dom[1].toFloat());
          break;
        default:
          break;
      }

    }
  }
  return {};
}


static State::ValueImpl fromBYJsonValue(
    const QJsonValue& val,
    State::ValueType type)
{
  using namespace State;
  if (val.isNull())
  {
    if (type == State::ValueType::Impulse)
      return State::ValueImpl{State::impulse_t{}};
    else
      return State::ValueImpl{};
  }

  switch (type)
  {
    case ValueType::NoValue:
      return State::ValueImpl{};
    case ValueType::Impulse:
      return State::ValueImpl{State::impulse_t{}};
    case ValueType::Int:
      return State::ValueImpl{val.toVariant().toInt()};
    case ValueType::Float:
      return State::ValueImpl{val.toVariant().toDouble()};
    case ValueType::Bool:
      return State::ValueImpl{val.toVariant().toBool()};
    case ValueType::String:
      return State::ValueImpl{val.toString().toStdString()};
    case ValueType::Char:
    {
      auto str = val.toString();
      if (!str.isEmpty())
        return State::ValueImpl{str[0].toLatin1()};
      return State::ValueImpl{char{}};
    }

    case ValueType::Tuple:
    {
      // Tuples are always tuples of numbers in this case.
      auto arr = val.toString().split(' ');
      State::tuple_t tuple;
      tuple.reserve(arr.size());

      for(const auto& val : arr)
      {
        tuple.push_back(val.toDouble());
      }

      return State::ValueImpl{std::move(tuple)};
    }
    case ValueType::Vec2f:
    case ValueType::Vec3f:
    case ValueType::Vec4f:
    default:
      return State::ValueImpl{};
  }
}

static const json_actions_t& actions()
{
  static json_actions_t acts{
    [] {
      json_actions_t a;
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("type"),
                  [&] (Device::Node& node, const QJsonValue& val) {
                    Device::AddressSettings& addr = node.get<Device::AddressSettings>();
                    addr.value = fromTextualType(val.toString());
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("description"),
                  [&] (Device::Node& node, const QJsonValue& val) {
                    Device::AddressSettings& addr = node.get<Device::AddressSettings>();
                    addr.extendedAttributes["description"] = val.toString();
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("valueDefault"),
                  [&] (Device::Node& node, const QJsonValue& val) {
                    Device::AddressSettings& addr = node.get<Device::AddressSettings>();
                    addr.value = fromBYJsonValue(val, addr.value.val.which());
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("priority"),
                  [&] (Device::Node& node, const QJsonValue& val) {
                    Device::AddressSettings& addr = node.get<Device::AddressSettings>();
                    addr.extendedAttributes["priority"] = val.toVariant().toInt();
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("rangeBounds"),
                  [&] (Device::Node& node, const QJsonValue& val) {
                    Device::AddressSettings& addr = node.get<Device::AddressSettings>();
                    addr.domain = fromJsonDomain(val.toString(), addr.value.val.which());
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("valueStepSize"),
                  [&] (Device::Node& node, const QJsonValue& val) {
                    Device::AddressSettings& addr = node.get<Device::AddressSettings>();
                    addr.extendedAttributes["valueStepSize"] = val.toVariant().toInt();
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("readonly"),
                  [&] (Device::Node& node, const QJsonValue& val) {
                    Device::AddressSettings& addr = node.get<Device::AddressSettings>();
                    if(val.toVariant().toInt() == 1)
                      addr.ioType = IOType::In;
                  }});
      return a;
    }()
  };

  return acts;
}

static void read_node(
    const QJsonObject& dom_element,
    Device::Node& thisNode)
{
  // If the nodes are objects, they're children
  // Else we have to parse them into the node's attributes.
  auto& acts = actions();

  // First search for type because value has to come afterwards.
  auto json_begin = dom_element.constBegin();
  auto json_end = dom_element.constEnd();
  for(auto it = json_begin; it != json_end; ++it)
  {
    const QJsonValue& val = it.value();
    if(val.isObject())
    {
      // It means that it's a children on which we recurse
      Device::AddressSettings cld;
      cld.name = it.key();

      auto& childNode = thisNode.emplace_back(cld, &thisNode);
      read_node(val.toObject(), childNode);
    }
    else
    {
      // It's a common attribute.
      auto act_it = acts.find(it.key());
      if(act_it != acts.end())
      {
        act_it.value()(thisNode, val);
      }
    }
  }
}

void loadDeviceFromBlueYetiJSON(
    const QString& filePath,
    Device::Node& rootNode)
{
  // ouverture d'un xml
  QFile theFile{filePath};
  if(!theFile.open(QIODevice::ReadOnly))
  {
    qDebug() << "Erreur : Impossible d'ouvrir le ficher JSON";
    theFile.close();
    return;
  }

  QJsonDocument qt_doc = QJsonDocument::fromJson(theFile.readAll());
  if(qt_doc.isNull())
  {
    return;
  }

  // The root is an object.
  const auto& obj = qt_doc.object();

  // It should have a single key which is the device object.
  auto it = obj.constBegin();
  if(it != obj.constEnd())
  {
    const auto& main_v = it.value();
    if(main_v.isObject())
    {
      const auto& main_obj = main_v.toObject();

      auto sub_it = main_obj.constBegin();
      auto sub_it_end = main_obj.constEnd();
      for(; sub_it != sub_it_end; sub_it++)
      {
        const auto& sub_it_val = sub_it.value();
        if(sub_it_val.isObject())
        {
          // It means that it's a children on which we recurse
          Device::AddressSettings cld;
          cld.name = it.key();

          auto& childNode = rootNode.emplace_back(cld, &rootNode);
          read_node(sub_it_val.toObject(), childNode);
        }
      }
    }
  }
}
}

