#include "XMLDeviceLoader.hpp"
#include <QtXml/QtXml>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

using namespace iscore;
static void convertFromDomElement(QDomElement dom_element, Node *parentNode)
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

    auto stringToVal = [] (const QString& str, const QString& type)
    {
        QVariant val;
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
            val = str;
            ok = true;
        }
        else
        {
            qDebug() << "Unknown type: " << type;
        }

        if(!ok)
            return QVariant{};

        return val;
    };

    iscore::AddressSettings addr;
    addr.name = name;

    if(dom_element.hasAttribute("type"))
    {
        const auto type = dom_element.attribute("type");

        if(dom_element.hasAttribute("valueDefault"))
        {
            const auto value = dom_element.attribute("valueDefault");
            addr.value.val = stringToVal(value, type);
        }
        else
        {
            if(type == "string")
            {
                addr.value.val = stringToVal("", type);
            }
            else
            {
                addr.value.val = stringToVal("0", type);
            }
        }

        if(dom_element.hasAttribute("service"))
        {
            const auto service = dom_element.attribute("service");
            if(service == "parameter")
                addr.ioType = IOType::InOut;
            /*
        else if(service == "")
            addr.ioType = IOType::In;
        else if(service == "")
            addr.ioType = IOType::Out;
        */
            else
                addr.ioType = IOType::Invalid;
        }

        addr.priority = dom_element.attribute("priority").toInt();
        addr.repetitionFilter = dom_element.attribute("repetitionsFilter").toInt();

        if(dom_element.hasAttribute("rangeBounds"))
        {
            QString bounds = dom_element.attribute("rangeBounds");
            QString minBound = bounds;
            minBound.truncate(bounds.indexOf(" "));
            bounds.remove(0, minBound.length() + 1); // contains max

            if(type == "decimal" || type == "integer")
            {
                addr.domain.min.val = stringToVal(minBound, type);
                addr.domain.max.val = stringToVal(bounds, type);
            }
        }

        if(dom_element.hasAttribute("rangeClipmode"))
        {
            const auto clipmode = dom_element.attribute("rangeClipmode");
            if(clipmode == "both")
            {
                addr.clipMode = iscore::ClipMode::Clip;
            }
        }
    }

    auto childNode = new Node{addr, parentNode};
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
        convertFromDomElement(dom_node, &node);
        dom_node = dom_node.nextSiblingElement("");
    }
}
