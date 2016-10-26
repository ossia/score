#include <Device/Node/DeviceNode.hpp>
#include <QDebug>
#include <qdom.h>
#include <QFile>
#include <QIODevice>

#include <QJsonDocument>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <State/Value.hpp>
#include "XMLDeviceLoader.hpp"


namespace Device
{
static State::Value stringToVal(const QString& str, const QString& type)
{
    State::Value val;
    bool ok = false;
    if(type == "integer")
    {
        val.val = str.toInt(&ok);
    }
    else if(type == "decimal")
    {
        val.val = str.toFloat(&ok);
    }
    else if(type == "boolean")
    {
        val.val = (str.toFloat(&ok) != 0);
    }
    else if(type == "string")
    {
        val.val = str;
        ok = true;
    }
    else
    {
        qDebug() << "Unknown type: " << type;
    }

    if(!ok)
        return State::Value{};

    return val;
}

static ossia::value stringToOssiaVal(const QString& str, const QString& type)
{
    ossia::value val;
    bool ok = false;
    if(type == "integer")
    {
        val = str.toInt(&ok);
    }
    else if(type == "decimal")
    {
        val = str.toFloat(&ok);
    }
    else if(type == "boolean")
    {
        val = (str.toFloat(&ok) != 0);
    }
    else if(type == "string")
    {
        val = str.toStdString();
        ok = true;
    }
    else
    {
        qDebug() << "Unknown type: " << type;
    }

    if(!ok)
        return {};

    return val;
}


static State::Value read_valueDefault(
        const QDomElement& dom_element,
        const QString& type)
{
    if(dom_element.hasAttribute("valueDefault"))
    {
        const auto value = dom_element.attribute("valueDefault");
        return stringToVal(value, type);
    }
    else
    {
        return stringToVal((type == "string") ? "" : "0", type);
    }
}


static auto read_service(const QDomElement& dom_element)
{
    using namespace iscore;
    if(dom_element.hasAttribute("service"))
    {
        const auto service = dom_element.attribute("service");
        if(service == "parameter")
            return Device::IOType::InOut;
        /*
    else if(service == "")
        addr.ioType = IOType::In;
    else if(service == "")
        addr.ioType = IOType::Out;
    */
        else
            return Device::IOType::Invalid;
    }

    return Device::IOType::Invalid;
}

static auto read_rangeBounds(
        const QDomElement& dom_element,
        const QString& type)
{
    ossia::net::domain domain;

    if(dom_element.hasAttribute("rangeBounds"))
    {
        QString bounds = dom_element.attribute("rangeBounds");
        QString minBound = bounds;
        minBound.truncate(bounds.indexOf(" "));
        bounds.remove(0, minBound.length() + 1); // contains max

        if(type == "decimal" || type == "integer")
        {
            auto v1 = stringToOssiaVal(minBound, type);
            auto v2 = stringToOssiaVal(bounds, type);
            domain = ossia::net::make_domain(v1, v2);
        }
    }

    return domain;
}

static auto read_rangeClipmode(const QDomElement& dom_element)
{
    if(dom_element.hasAttribute("rangeClipmode"))
    {
        const auto clipmode = dom_element.attribute("rangeClipmode");
        if(clipmode == "both")
        {
            return Device::ClipMode::Clip;
        }
    }
    return Device::ClipMode::Free;
}

static void convertFromDomElement(const QDomElement& dom_element, Device::Node &parentNode)
{
    QDomElement dom_child = dom_element.firstChildElement("");
    QString name;

    if(dom_element.hasAttribute("address"))
    {
        name = dom_element.attribute("address");
    }
    else
    {
        name = dom_element.tagName();
    }

    Device::AddressSettings addr;
    addr.name = name;

    if(dom_element.hasAttribute("type"))
    {
        const auto type = dom_element.attribute("type");
        addr.value = read_valueDefault(dom_element, type);
        addr.ioType = read_service(dom_element);

        addr.priority = dom_element.attribute("priority").toInt();
        addr.repetitionFilter = dom_element.attribute("repetitionsFilter").toInt();

        addr.domain = read_rangeBounds(dom_element, type);
        addr.clipMode = read_rangeClipmode(dom_element);
    }

    auto& childNode = parentNode.emplace_back(addr, &parentNode);

    while(!dom_child.isNull() && dom_element.hasChildNodes())
    {
        convertFromDomElement(dom_child, childNode);

        dom_child = dom_child.nextSibling().toElement();
    }
    return;
}

bool loadDeviceFromXML(const QString &filePath, Device::Node &node)
{
    // ouverture d'un xml
    QFile doc_xml(filePath);
    if(!doc_xml.open(QIODevice::ReadOnly))
    {
        qDebug() << "Erreur : Impossible d'ouvrir le ficher XML";
        doc_xml.close();
        return false;
    }

    QDomDocument domDoc;
    if(!domDoc.setContent(&doc_xml))
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

    while(!dom_node.isNull())
    {
        convertFromDomElement(dom_node, node);
        dom_node = dom_node.nextSiblingElement("");
    }

    return true;
}


bool loadDeviceFromJSON(const QString &filePath, Device::Node &node)
{
    QFile doc{filePath};
    if(!doc.open(QIODevice::ReadOnly))
    {
        qDebug() << "Erreur : Impossible d'ouvrir le ficher Device";
        doc.close();
        return false;
    }

    auto json = QJsonDocument::fromJson(doc.readAll());
    if(!json.isObject())
    {
        qDebug() << "Erreur : Impossible de charger le ficher Device";
        doc.close();
        return false;
    }

    doc.close();

    auto obj = json.object();
    JSONObject::Deserializer des{obj};
    des.writeTo(node);

    return true;
}
}
