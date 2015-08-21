#include "LoadXML.hpp"
#include "Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Node/Node.hpp>
#include <DeviceExplorer/Protocol/ProtocolList.hpp>
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>

#include <Plugin/DeviceExplorerPlugin.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QMessageBox>
#include <QFile>
#include <QtXml/QtXml>
#include <QApplication>
#include <QWindow>

using namespace iscore;
static void convertFromDomElement(QDomElement dom_element, Node* parentNode)
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

// Should be in the presenter instead ??
static void loadDeviceFromXML(const QString& filePath, Node& node)
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

LoadXML::LoadXML(
        ObjectPath&& device_tree,
        const DeviceSettings& parameters,
        const QString &filePath):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                commandName(),
                                description()},
    m_deviceTree{device_tree},
    m_deviceNode{parameters}
{
    // We load the node in the constructor,
    //so that it can be sent via the network afterwards.

    loadDeviceFromXML(filePath, m_deviceNode);
}

void LoadXML::undo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    explorer.deviceModel()->list().removeDevice(explorer.index(m_row, 0, QModelIndex()).data().toString());
    explorer.removeRow(m_row);
}

void LoadXML::redo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    Q_ASSERT(explorer.deviceModel());

    try {
        // Instantiate a real device.
        auto proto = SingletonProtocolList::instance().protocol(m_deviceNode.deviceSettings().protocol);
        auto newdev = proto->makeDevice(m_deviceNode.deviceSettings());

        explorer.deviceModel()->list().addDevice(newdev);

        // TODO create all the "real" addresses
    }
    catch(std::runtime_error e)
    {
        QMessageBox::warning(QApplication::activeWindow(),
                             QObject::tr("Error loading device"),
                             m_deviceNode.deviceSettings().name + ": " + QString::fromLatin1(e.what()));
    }

    // Put it in the tree.
    m_row = explorer.addDevice(new Node{m_deviceNode});
}

int LoadXML::deviceRow() const
{
    return m_row;
}

void LoadXML::serializeImpl(QDataStream& d) const
{
    d << m_deviceTree;
    d << m_deviceNode;
    d << m_row;
}

void LoadXML::deserializeImpl(QDataStream& d)
{
    d >> m_deviceTree;
    d >> m_deviceNode;
    d >> m_row;
}
