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
AddDevice::AddDevice(ObjectPath&& device_tree, const DeviceSettings& parameters):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                commandName(),
                                description()},
    m_deviceTree{device_tree},
    m_parameters(parameters)
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

    d << m_row;
}

void AddDevice::deserializeImpl(QDataStream& d)
{
    d >> m_deviceTree;
    d >> m_parameters;

    d >> m_row;
}
