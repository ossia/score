// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JamomaDeviceLoader.hpp"

#include <score/tools/File.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QDebug>
#include <QDomAttr>
#include <QDomDocument>
#include <QDomEntity>
#include <QFile>

namespace Device
{
// Note : there are plenty of things to refactor between
// the XML and JSON method, since they actually use almost the
// same format.

static ossia::value stringToVal(const QString& str, const QString& type)
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
  else if (type == "none")
  {
    val = ossia::impulse{};
    ok = true;
  }
  else
  {
    qDebug() << "Unknown type: " << type;
  }

  if (!ok)
    return ossia::value{};

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
  else if (type == "none")
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

static ossia::value
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

static std::optional<ossia::access_mode>
read_service(const QDomElement& dom_element)
{
  using namespace score;
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
      return std::nullopt;
  }

  return std::nullopt;
}

static auto
read_rangeBounds(const QDomElement& dom_element, const QString& type)
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

    addr.repetitionFilter
        = rfl ? ossia::repetition_filter::ON : ossia::repetition_filter::OFF;

    addr.domain = read_rangeBounds(dom_element, type);
    addr.clipMode = read_rangeClipmode(dom_element);

    if (!addr.ioType)
    {
      if (addr.value.valid())
      {
        addr.ioType = ossia::access_mode::BI;
      }
    }

    if (dom_element.previousSibling().isComment())
    {
      auto desc = dom_element.previousSibling().nodeValue();
      if (desc.startsWith("\""))
        desc.remove(0, 1);
      if (desc.endsWith("\""))
        desc.chop(1);
      ossia::net::set_description(addr, desc.toStdString());
    }
  }

  auto& childNode = parentNode.emplace_back(addr, &parentNode);

  while (!dom_child.isNull() && dom_element.hasChildNodes())
  {
    convertFromDomElement(dom_child, childNode);

    auto ns = dom_child.nextSibling();
    while (ns.isComment())
    {
      ns = ns.nextSibling();
    }
    dom_child = ns.toElement();
  }
  return;
}

bool loadDeviceFromXML(const QString& filePath, Device::Node& node)
{
  // Open the Jamoma XML device
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

  // Read the Jamoma XML format
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

}
