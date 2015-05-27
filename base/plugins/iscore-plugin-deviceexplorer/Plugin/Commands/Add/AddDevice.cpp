#include "AddDevice.hpp"
#include "Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Node/Node.hpp>
#include <DeviceExplorer/Protocol/ProtocolList.hpp>
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>

#include <Plugin/DeviceExplorerPlugin.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <QFile>
#include <QtXml/QtXml>

void convertFromDomElement(QDomElement dom_element, Node* parentNode)
{
    QDomElement dom_child = dom_element.firstChildElement("");
    QString name;
    QString valueType{"Float"};
    QString value{"0"};
    int priority;
    float min{0};
    float max{100};

    if(dom_element.hasAttribute("address"))
    {
        name = dom_element.attribute("address");
    }
    else
    {
        name = dom_element.tagName();
    }

    priority = dom_element.attribute("priority").toInt();

    QString bounds = dom_element.attribute("rangeBounds");
    QString minB = bounds;
    minB.truncate(bounds.indexOf(" "));
    bounds.remove(0, minB.length() + 1);
    min = minB.toFloat();
    max = bounds.toFloat();

    value = dom_element.attribute("valueDefault");

    Node* treeNode = new Node{name, parentNode};
    treeNode->setValueType(valueType);
    treeNode->setIOType(IOType::InOut);
    treeNode->setPriority(priority);
    treeNode->setValue(value);
    treeNode->setMaxValue(max);
    treeNode->setMinValue(min);

    while(!dom_child.isNull() && dom_element.hasChildNodes())
    {
        convertFromDomElement(dom_child, treeNode);

        dom_child = dom_child.nextSibling().toElement();
    }
    return;
}

Node* makeDeviceNode(const DeviceSettings& device, const QString& filePath)
{
    Node* node = new Node(device, device.name, nullptr);
    if (filePath.isEmpty())
    {
        //DEBUG: arbitrary population of the tree


        if(device.protocol == "Minuit" || device.protocol == "OSC")
        {
            Node* node1 = new Node("debug1", node);
            node1->setValueType("Int");
            node1->setValue("10");
            node1->setIOType(IOType::In);
            node1->setMinValue(0);
            node1->setMaxValue(10);
            node1->setPriority(1);

            Node* node2 = new Node("debug2", node);
            node2->setValueType("Float");
            node2->setValue("13.7");
            node2->setIOType(IOType::Out);
            node2->setMinValue(0.f);
            node2->setMaxValue(76.f);
            node2->setPriority(2);
        }

        if(device.protocol == "OSC" || device.protocol == "MIDI")
        {
            Node* node3 = new Node("debug3", node);
            node3->setValueType("Float");
            node3->setValue("13");
            node3->setIOType(IOType::InOut);
            node3->setMinValue(0.f);
            node3->setMaxValue(100.f);
            node3->setPriority(2);

            Node* node4 = new Node("debug4", node3);
            node4->setValueType("Float");
            node4->setValue("11");
            node4->setIOType(IOType::InOut);
            node4->setMinValue(1.f);
            node4->setMaxValue(78.f);
            node4->setPriority(7);

            if(device.protocol == "OSC")
            {
                Node* node5 = new Node("debug5", node4);
                node5->setValueType("Float");
                node5->setValue("777");
                node5->setIOType(IOType::In);
                node5->setMinValue(1.f);
                node5->setMaxValue(3.f);
                node5->setPriority(3);

                Node* node6 = new Node("debug6", node5);
                node6->setValueType("Float");
                node6->setValue("777");
                node6->setIOType(IOType::In);
                node6->setMinValue(1.f);
                node6->setMaxValue(3.f);
                node6->setPriority(3);

                Node* node7 = new Node("debug7", node5);
                node7->setValueType("Float");
                node7->setValue("754");
                node7->setIOType(IOType::Out);
                node7->setMinValue(1.33f);
                node7->setMaxValue(2.3f);
                node7->setPriority(33);
            }
        }
    }
    else
    {
        // ouverture d'un xml
        QFile doc_xml(filePath);
        if(!doc_xml.open(QIODevice::ReadOnly))
        {
            qDebug() << "Erreur : Impossible d'ouvrir le ficher XML";
            doc_xml.close();
            return node;
        }
        QDomDocument* domDoc = new QDomDocument;
        if(!domDoc->setContent(&doc_xml))
        {
            qDebug() << "Erreur : Impossible de charger le ficher XML";
            doc_xml.close();
            return node;
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

    return node;
}


const char* AddDevice::className() { return "AddDevice"; }
QString AddDevice::description() { return QObject::tr("Add a device"); }
AddDevice::AddDevice(ObjectPath&& device_tree, const DeviceSettings& parameters, const QString &filePath):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                className(),
                                description()},
    m_deviceTree{device_tree},
    m_parameters(parameters),
    m_filePath{filePath}
{

}

void AddDevice::undo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    explorer.removeRow(m_row);
    explorer.deviceModel()->list().removeDevice(explorer.index(m_row, 0, QModelIndex()).data().toString());
}

void AddDevice::redo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    // Instantiate a real device.
    auto proto = SingletonProtocolList::instance().protocol(m_parameters.protocol);
    explorer.deviceModel()->list().addDevice(proto->makeDevice(m_parameters));

    // Put it in the tree.
    auto node = makeDeviceNode(m_parameters, m_filePath);
    m_row = explorer.addDevice(node);
}

void AddDevice::serializeImpl(QDataStream& d) const
{
    d << m_deviceTree;
    d << m_parameters;
    d << m_filePath;

    d << m_row;
}

void AddDevice::deserializeImpl(QDataStream& d)
{
    d >> m_deviceTree;
    d >> m_parameters;
    d >> m_filePath;

    d >> m_row;
}
