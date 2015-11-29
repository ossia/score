#include <Device/Node/DeviceNode.hpp>
#include <QDebug>
#include <qdom.h>
#include <QFile>
#include <QIODevice>

#include "Device/Address/AddressSettings.hpp"
#include "Device/Address/ClipMode.hpp"
#include "Device/Address/Domain.hpp"
#include "Device/Address/IOType.hpp"
#include <State/Value.hpp>
#include "XMLDeviceLoader.hpp"

static iscore::Value stringToVal(const QString& str, const QString& type)
{
    iscore::Value val;
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
        return iscore::Value{};

    return val;
}

static iscore::Value read_valueDefault(
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
            return IOType::InOut;
        /*
    else if(service == "")
        addr.ioType = IOType::In;
    else if(service == "")
        addr.ioType = IOType::Out;
    */
        else
            return IOType::Invalid;
    }

    return IOType::Invalid;
}

static auto read_rangeBounds(
        const QDomElement& dom_element,
        const QString& type)
{
    iscore::Domain domain;

    if(dom_element.hasAttribute("rangeBounds"))
    {
        QString bounds = dom_element.attribute("rangeBounds");
        QString minBound = bounds;
        minBound.truncate(bounds.indexOf(" "));
        bounds.remove(0, minBound.length() + 1); // contains max

        if(type == "decimal" || type == "integer")
        {
            domain.min = stringToVal(minBound, type);
            domain.max = stringToVal(bounds, type);
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
            return iscore::ClipMode::Clip;
        }
    }
    return iscore::ClipMode{};
}

using namespace iscore;
static void convertFromDomElement(const QDomElement& dom_element, Node &parentNode)
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

    iscore::AddressSettings addr;
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

void loadDeviceFromXML(const QString &filePath, Node &node)
{
    // ouverture d'un xml
    QFile doc_xml(filePath);
    if(!doc_xml.open(QIODevice::ReadOnly))
    {
        qDebug() << "Erreur : Impossible d'ouvrir le ficher XML";
        doc_xml.close();
    }
    QDomDocument* domDoc = new QDomDocument;
    if(!domDoc->setContent(&doc_xml))
    {
        qDebug() << "Erreur : Impossible de charger le ficher XML";
        doc_xml.close();
    }
    doc_xml.close();

    // extraction des données

    QDomElement doc = domDoc->documentElement();
    QDomElement application = doc.firstChildElement("application");
    QDomElement dom_node = application.firstChildElement("");

    while(!dom_node.isNull())
    {
        convertFromDomElement(dom_node, node);
        dom_node = dom_node.nextSiblingElement("");
    }
}
