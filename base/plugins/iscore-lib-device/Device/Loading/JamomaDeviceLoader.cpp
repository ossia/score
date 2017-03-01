#include "JamomaDeviceLoader.hpp"
#include <QFile>
#include <QJsonDocument>
#include <qdom.h>
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
// Note : there are plenty of things to refactor between
// the XML and JSON method, since they actually use almost the
// same format.

static State::Value stringToVal(const QString& str, const QString& type)
{
  State::Value val;
  bool ok = false;
  if (type == "integer")
  {
    val.val = str.toInt(&ok);
  }
  else if (type == "decimal")
  {
    val.val = str.toFloat(&ok);
  }
  else if (type == "boolean")
  {
    val.val = (str.toFloat(&ok) != 0);
  }
  else if (type == "string")
  {
    val.val = str.toStdString();
    ok = true;
  }
  else if (type == "array")
  {
    val.val = State::tuple_t{};
    // TODO
    ok = true;
  }
  else if(type == "none")
  {
    val.val = ossia::impulse{};
    ok = true;
  }
  else
  {
    qDebug() << "Unknown type: " << type;
  }

  if (!ok)
    return State::Value{};

  return val;
}

static ossia::value stringToOssiaVal(const QString& str, const QString& type)
{
  ossia::value val;
  bool ok = false;
  if (type == "integer")
  {
    val = str.toInt(&ok);
  }
  else if (type == "decimal")
  {
    val = str.toFloat(&ok);
  }
  else if (type == "boolean")
  {
    val = (str.toFloat(&ok) != 0);
  }
  else if (type == "string")
  {
    val = str.toStdString();
    ok = true;
  }
  else if (type == "array")
  {
    val = std::vector<ossia::value>{};
    // TODO
    ok = true;
  }
  else if(type == "none")
  {
    val = ossia::impulse{};
    ok = true;
  }
  else
  {
    qDebug() << "Unknown type: " << type;
  }

  /*
  if (!ok)
    return {};
  */
  return val;
}

static State::Value
read_valueDefault(const QDomElement& dom_element, const QString& type)
{
  if (dom_element.hasAttribute("valueDefault"))
  {
    const auto value = dom_element.attribute("valueDefault");
    return stringToVal(value, type);
  }
  else
  {
    return stringToVal((type == "string") ? "" : "0", type);
  }
}

static optional<ossia::access_mode> read_service(
    const QDomElement& dom_element)
{
  using namespace iscore;
  if (dom_element.hasAttribute("service"))
  {
    const auto service = dom_element.attribute("service");
    if (service == "parameter")
      return ossia::access_mode::BI;
    /*
else if(service == "")
    addr.ioType = ossia::access_mode::GET;
else if(service == "")
    addr.ioType = ossia::access_mode::SET;
*/
    else
      return ossia::none;
  }

  return ossia::none;
}

static auto
read_rangeBounds(
    const QDomElement& dom_element,
    const QString& type)
{
  ossia::domain domain;

  if (dom_element.hasAttribute("rangeBounds"))
  {
    QString bounds = dom_element.attribute("rangeBounds");
    QString minBound = bounds;
    minBound.truncate(bounds.indexOf(" "));
    bounds.remove(0, minBound.length() + 1); // contains max

    if (type == "decimal" || type == "integer")
    {
      auto v1 = stringToOssiaVal(minBound, type);
      auto v2 = stringToOssiaVal(bounds, type);
      domain = ossia::make_domain(v1, v2);
    }
  }

  return domain;
}

static auto read_rangeClipmode(const QDomElement& dom_element)
{
  if (dom_element.hasAttribute("rangeClipmode"))
  {
    const auto clipmode = dom_element.attribute("rangeClipmode");
    if (clipmode == "both")
    {
      return ossia::bounding_mode::CLIP;
    }
  }
  return ossia::bounding_mode::FREE;
}

static void
convertFromDomElement(const QDomElement& dom_element, Device::Node& parentNode)
{
  QDomElement dom_child = dom_element.firstChildElement("");
  QString name;

  if (dom_element.hasAttribute("address"))
  {
    name = dom_element.attribute("address");
  }
  else
  {
    name = dom_element.tagName();
  }

  Device::AddressSettings addr;
  addr.name = name;

  if (dom_element.hasAttribute("type"))
  {
    const auto type = dom_element.attribute("type");
    addr.value = read_valueDefault(dom_element, type);
    addr.ioType = read_service(dom_element);

    ossia::net::set_priority(addr, dom_element.attribute("priority").toInt());
    auto rfl = dom_element.attribute("repetitionsFilter").toInt();

    addr.repetitionFilter = rfl ? ossia::repetition_filter::ON : ossia::repetition_filter::OFF;

    addr.domain = read_rangeBounds(dom_element, type);
    addr.clipMode = read_rangeClipmode(dom_element);

    if(!addr.ioType)
    {
      if(addr.value.val.isValid())
      {
        addr.ioType = ossia::access_mode::BI;
      }
    }

    if(dom_element.previousSibling().isComment())
    {
      auto desc = dom_element.previousSibling().nodeValue();
      if(desc.startsWith("\"")) desc.remove(0, 1);
      if(desc.endsWith("\"")) desc.chop(1);
      ossia::net::set_description(addr, desc.toStdString());
    }
  }

  auto& childNode = parentNode.emplace_back(addr, &parentNode);

  while (!dom_child.isNull() && dom_element.hasChildNodes())
  {
    convertFromDomElement(dom_child, childNode);

    auto ns = dom_child.nextSibling();
    while(ns.isComment())
    {
      ns = ns.nextSibling();
    }
    dom_child = ns.toElement();

  }
  return;
}

bool loadDeviceFromXML(const QString& filePath, Device::Node& node)
{
  // ouverture d'un xml
  QFile doc_xml(filePath);
  if (!doc_xml.open(QIODevice::ReadOnly))
  {
    qDebug() << "Erreur : Impossible d'ouvrir le ficher XML";
    doc_xml.close();
    return false;
  }

  QDomDocument domDoc;
  if (!domDoc.setContent(&doc_xml))
  {
    qDebug() << "Erreur : Impossible de charger le ficher XML";
    doc_xml.close();
    return false;
  }

  doc_xml.close();

  // extraction des données

  QDomElement doc = domDoc.documentElement();
  QDomElement application = doc.firstChildElement("application");
  QDomElement dom_node = application.firstChildElement("");

  while (!dom_node.isNull())
  {
    convertFromDomElement(dom_node, node);
    dom_node = dom_node.nextSiblingElement("");
  }

  return true;
}



using json_actions_t = iscore::hash_map<
QString,
std::function<
void(
Device::AddressSettings& node,
const QJsonValue& val)
>
>;

static State::Value fromJamomaTextualType(const QString& str)
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

static optional<ossia::net::instance_bounds> fromJamomaInstanceBounds(const QString& str)
{
  if(!str.isEmpty())
  {
    auto inst = str.split(' ');
    if(inst.size() == 2)
    {
      return ossia::net::instance_bounds(inst[0].toInt(), inst[1].toInt());
    }
  }
  return ossia::none;
}

static ossia::domain fromJamomaJsonDomain(
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
          return ossia::make_domain(dom[0].toInt(), dom[1].toInt());
          break;
        case State::ValueType::Float:
        case State::ValueType::Tuple:
        case State::ValueType::Vec2f:
        case State::ValueType::Vec3f:
        case State::ValueType::Vec4f:
          return ossia::make_domain(dom[0].toFloat(), dom[1].toFloat());
          break;
        default:
          break;
      }

    }
  }
  return {};
}


static State::ValueImpl fromJamomaJsonValue(
    const QJsonValue& val,
    State::ValueType type)
{
  using namespace State;
  if (val.isNull())
  {
    if (type == State::ValueType::Impulse)
      return State::ValueImpl{State::impulse{}};
    else
      return State::ValueImpl{};
  }

  switch (type)
  {
    case ValueType::NoValue:
      return State::ValueImpl{};
    case ValueType::Impulse:
      return State::ValueImpl{State::impulse{}};
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
                  [] (Device::AddressSettings& addr, const QJsonValue& val) {
                    addr.value = fromJamomaTextualType(val.toString());
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("description"),
                  [] (Device::AddressSettings& addr, const QJsonValue& val) {
                    ossia::net::set_description(addr, val.toString().toStdString());
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("valueDefault"),
                  [] (Device::AddressSettings& addr, const QJsonValue& val) {
                    addr.value = fromJamomaJsonValue(val, addr.value.val.which());
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("priority"),
                  [] (Device::AddressSettings& addr, const QJsonValue& val) {
                    ossia::net::set_priority(addr, val.toVariant().toInt());
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("rangeBounds"),
                  [] (Device::AddressSettings& addr, const QJsonValue& val) {
                    addr.domain = fromJamomaJsonDomain(val.toString(), addr.value.val.which());
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("valueStepSize"),
                  [] (Device::AddressSettings& addr, const QJsonValue& val) {
                    ossia::net::set_value_step_size(addr, val.toVariant().toInt());
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("readonly"),
                  [] (Device::AddressSettings& addr, const QJsonValue& val) {
                    auto v = val.toVariant().toInt();
                    if(v == 1)
                      addr.ioType = ossia::access_mode::GET;
                    else
                      addr.ioType = ossia::access_mode::BI;
                  }});
      a.emplace(json_actions_t::value_type{
                  QStringLiteral("instanceBounds"),
                  [] (Device::AddressSettings& addr, const QJsonValue& val) {
                    // TODO there is a memory corruption error when
                    // doing it directly with val.toString(), maybe a Qt bug ?
                    QString str = val.toString();
                    ossia::net::set_instance_bounds(addr, fromJamomaInstanceBounds(str));
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
  Device::AddressSettings& addr = thisNode.get<Device::AddressSettings>();
  // If the nodes are objects, they're children
  // Else we have to parse them into the node's attributes.
  auto& acts = actions();

  // First search for type because value has to come afterwards.
  auto type_it = dom_element.find("type");
  if(type_it != dom_element.end())
  {
    addr.value = fromJamomaTextualType(type_it->toString());
  }
  auto json_begin = dom_element.constBegin();
  auto json_end = dom_element.constEnd();
  for(auto it = json_begin; it != json_end; ++it)
  {
    if(it == type_it)
      continue;

    const QJsonValue& val = it.value();
    if(val.isObject())
    {
      // It means that it's a children on which we recurse
      Device::AddressSettings cld;
      cld.ioType = ossia::access_mode::BI;
      cld.name = it.key();

      auto& childNode = thisNode.emplace_back(std::move(cld), &thisNode);
      read_node(val.toObject(), childNode);
    }
    else
    {
      // It's a common attribute.
      auto act_it = acts.find(it.key());
      if(act_it != acts.end())
      {
        act_it.value()(addr, val);
      }
    }
  }
}

bool loadDeviceFromJamomaJSON(
    const QString& filePath,
    Device::Node& rootNode)
try
{
  QFile theFile{filePath};
  if(!theFile.open(QIODevice::ReadOnly))
  {
    qDebug() << "Error : unable to open the JSON";
    theFile.close();
    return false;
  }

  QJsonDocument qt_doc = QJsonDocument::fromJson(theFile.readAll());
  if(qt_doc.isNull())
  {
    return false;
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
          cld.ioType = ossia::access_mode::BI;
          cld.name = sub_it.key();

          auto& childNode = rootNode.emplace_back(cld, &rootNode);
          read_node(sub_it_val.toObject(), childNode);
        }
        else
        {
          // TODO handle the "metadata" attributes...
        }
      }
    }
  }

  return true;
}
catch(...)
{
return false;
}
}


