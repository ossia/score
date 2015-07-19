#include "AddDevice.hpp"
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
// TODO check xml loading.
void convertFromDomElement(QDomElement dom_element, Node* parentNode)
{
    QDomElement dom_child = dom_element.firstChildElement("");
    QString name;
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

    iscore::AddressSettings addr;

    bool ok = false;
    addr.value.val = value.toUInt(&ok);
    if(!ok)
    {
        addr.value.val = value.toFloat(&ok);
        if(!ok)
        {
            addr.value.val = value.toInt(&ok);
            if(!ok)
            {
                addr.value.val = value;
            }
        }
    }
    addr.ioType = IOType::InOut;

    //treeNode->setPriority(priority);
    //treeNode->setMaxValue(max);
    //treeNode->setMinValue(min);

    while(!dom_child.isNull() && dom_element.hasChildNodes())
    {
        convertFromDomElement(dom_child, new Node{addr, parentNode});

        dom_child = dom_child.nextSibling().toElement();
    }
    return;
}

/*
Node* loadfromxml(const QString& filePath)
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
 */

AddDevice::AddDevice(ObjectPath&& device_tree, const DeviceSettings& parameters, const QString &filePath):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                commandName(),
                                description()},
    m_deviceTree{device_tree},
    m_parameters(parameters),
    m_filePath{filePath}
{

}

void AddDevice::undo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    explorer.deviceModel()->list().removeDevice(explorer.index(m_row, 0, QModelIndex()).data().toString());
    explorer.removeRow(m_row);
}

void AddDevice::redo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    Q_ASSERT(explorer.deviceModel());

    try {
        // Instantiate a real device.
        auto proto = SingletonProtocolList::instance().protocol(m_parameters.protocol);
        auto newdev = proto->makeDevice(m_parameters);
        explorer.deviceModel()->list().addDevice(newdev);
    }
    catch(std::runtime_error e)
    {
        QMessageBox::warning(QApplication::activeWindow(),
                             QObject::tr("Error loading device"),
                             m_parameters.name + ": " + QString::fromLatin1(e.what()));
    }


    // Put it in the tree.
    m_row = explorer.addDevice(new Node(m_parameters, nullptr));
}

int AddDevice::deviceRow() const
{
    return m_row;
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
