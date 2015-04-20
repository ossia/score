#include "AddDevice.hpp"
#include "Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Node/Node.hpp>
#include <DeviceExplorer/Protocol/ProtocolList.hpp>
#include <DeviceExplorer/Protocol/ProtocolFactoryInterface.hpp>

#include <Plugin/DeviceExplorerPlugin.hpp>

Node* makeDeviceNode(const DeviceSettings& device)
{
    Node* node = new Node(device, device.name, nullptr);
    {
        //DEBUG: arbitrary population of the tree

        if(device.protocol == "Minuit" || device.protocol == "OSC")
        {
            Node* node1 = new Node("debug1", node);
            node1->setValueType("Int");
            node1->setValue("10");
            node1->setIOType(Node::In);
            node1->setMinValue(0);
            node1->setMaxValue(10);
            node1->setPriority(1);

            Node* node2 = new Node("debug2", node);
            node2->setValueType("Float");
            node2->setValue("13.7");
            node2->setIOType(Node::Out);
            node2->setMinValue(0.f);
            node2->setMaxValue(76.f);
            node2->setPriority(2);
        }

        if(device.protocol == "OSC" || device.protocol == "MIDI")
        {
            Node* node3 = new Node("debug3", node);
            node3->setValueType("Float");
            node3->setValue("13");
            node3->setIOType(Node::InOut);
            node3->setMinValue(0.f);
            node3->setMaxValue(100.f);
            node3->setPriority(2);

            Node* node4 = new Node("debug4", node3);
            node4->setValueType("Float");
            node4->setValue("11");
            node4->setIOType(Node::InOut);
            node4->setMinValue(1.f);
            node4->setMaxValue(78.f);
            node4->setPriority(7);

            if(device.protocol == "OSC")
            {
                Node* node5 = new Node("debug5", node4);
                node5->setValueType("Float");
                node5->setValue("777");
                node5->setIOType(Node::In);
                node5->setMinValue(1.f);
                node5->setMaxValue(3.f);
                node5->setPriority(3);

                Node* node6 = new Node("debug6", node5);
                node6->setValueType("Float");
                node6->setValue("777");
                node6->setIOType(Node::In);
                node6->setMinValue(1.f);
                node6->setMaxValue(3.f);
                node6->setPriority(3);

                Node* node7 = new Node("debug7", node5);
                node7->setValueType("Float");
                node7->setValue("754");
                node7->setIOType(Node::Out);
                node7->setMinValue(1.33f);
                node7->setMaxValue(2.3f);
                node7->setPriority(33);
            }
        }
    }

    return node;
}

const char* AddDevice::className() { return "AddDevice"; }
QString AddDevice::description() { return "TODO"; }
AddDevice::AddDevice(ObjectPath&& device_tree, const DeviceSettings& parameters):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                className(),
                                description()},
    m_deviceTree{device_tree},
    m_parameters(parameters)
{

}

void AddDevice::undo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    explorer->removeRow(m_row);

    SingletonDeviceList::instance().removeDevice(m_parameters.name);
}

void AddDevice::redo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();

    // Instantiate a real device.
    auto proto = SingletonProtocolList::instance().protocol(m_parameters.protocol);
    auto dev = proto->makeDevice(m_parameters);
    SingletonDeviceList::instance().addDevice(dev);

    // Put it in the tree.
    auto node = makeDeviceNode(m_parameters);
    m_row = explorer->addDevice(node);
}

bool AddDevice::mergeWith(const iscore::Command* other)
{
    return false;
}

void AddDevice::serializeImpl(QDataStream&) const
{
    qDebug() << Q_FUNC_INFO << "TODO";
}

void AddDevice::deserializeImpl(QDataStream&)
{
    qDebug() << Q_FUNC_INFO << "TODO";
}
