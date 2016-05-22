#pragma once
#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <QAbstractItemModel>
#include <QList>
#include <qnamespace.h>
#include <QPair>
#include <QStack>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <Device/Node/DeviceNode.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <Explorer/Explorer/Column.hpp>
#include <iscore_plugin_deviceexplorer_export.h>
class QMimeData;
class QObject;
namespace iscore
{
    class CommandStackFacade;
}
namespace Device {
struct DeviceSettings;
struct AddressSettings;
}


namespace Explorer
{
class ListeningManager;
class DeviceDocumentPlugin;
class DeviceEditDialog;
class DeviceExplorerView;
class DeviceExplorerWidget;

/**
 * @brief The SelectedNodes struct
 *
 * When we make a selection, we have to differentiate
 * two things :
 *  - we want to select recursively all the PARAMETERS of what
 *    was selected by the user.
 *  - we want to select non-recursively all the MESSAGES that were
 *    explicitely selected by the user.
 * This struct allows this separation.
 */
struct ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT SelectedNodes
{
     /**
     * @brief parents The topmost parents of the selected parameters
     */
    QList<Device::Node*> parents;

    /**
     * @brief messages The selected messages
     */
    QList<Device::Node*> messages;
};


class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceExplorerModel final :
        public Device::NodeBasedItemModel
{
        Q_OBJECT

    public:

        explicit DeviceExplorerModel(
                DeviceDocumentPlugin&,
                QObject* parent);

        ~DeviceExplorerModel();

        Device::Node& rootNode() override
        { return m_rootNode; }

        const Device::Node& rootNode() const override
        { return m_rootNode; }

        void setView(DeviceExplorerView* v)
        {
            m_view = v;
        }

        // The class that does the link with the low-level implementation of the
        // devices. This is here so that commands don't need to save
        // at the same time a path to the device explorer, and the device doc plugin.
        DeviceDocumentPlugin& deviceModel() const;
        QModelIndexList selectedIndexes() const;

        const iscore::CommandStackFacade& commandStack() const
        { return m_cmdQ; }

        // Returns the row (useful for undo)
        int addDevice(Device::Node&& deviceNode);
        int addDevice(const Device::Node& deviceNode);
        void updateDevice(
                const QString &name,
                const Device::DeviceSettings& dev);

        void addAddress(
                Device::Node * parentNode,
                const Device::AddressSettings& addressSettings,
                int row);
        void updateAddress(
                Device::Node * node,
                const Device::AddressSettings& addressSettings);

        void addNode(
                Device::Node* parentNode,
                Device::Node&& child,
                int row);

        void updateValue(Device::Node* n,
                const State::Value& v);

        // Checks if the settings can be added; if not,
        // trigger a dialog to edit them as wanted.
        // Returns true if the device is to be added, false if
        // it should not be added.
        bool checkDeviceInstantiatable(
                Device::DeviceSettings& n);
        bool tryDeviceInstantiation(
                Device::DeviceSettings&,
                DeviceEditDialog&);

        bool checkAddressInstantiatable(
                Device::Node& parent,
                const Device::AddressSettings& addr);

        bool checkAddressEditable(
                Device::Node& parent,
                const Device::AddressSettings& before,
                const Device::AddressSettings& after);

        int columnCount() const;
        QStringList getColumns() const;
        bool isEmpty() const;
        bool isDevice(QModelIndex index) const;
        bool hasCut() const;

        void debug_printIndexes(const QModelIndexList& indexes);

        int columnCount(const QModelIndex& parent) const override;

        QVariant getData(Device::NodePath node, Column column, int role);
        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        Qt::ItemFlags flags(const QModelIndex& index) const override;

        bool setData(const QModelIndex& index, const QVariant& value, int role) override;
        bool setHeaderData(int, Qt::Orientation, const QVariant&, int = Qt::EditRole) override;

        void editData(const Device::NodePath &path, Column column, const State::Value& value, int role);

        Qt::DropActions supportedDropActions() const override;
        Qt::DropActions supportedDragActions() const override;
        QStringList mimeTypes() const override;
        QMimeData* mimeData(const QModelIndexList& indexes) const override;
        bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
        bool dropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

        QModelIndex convertPathToIndex(const Device::NodePath& path);

        SelectedNodes uniqueSelectedNodes(const QModelIndexList& indexes) const; // Note : filters so that only parents are given.

    protected:
        void debug_printPath(const Device::NodePath& path);

        typedef QPair<Device::Node*, bool> CutElt;
        QStack<CutElt> m_cutNodes;
        bool m_lastCutNodeIsCopied;

    private:
        DeviceDocumentPlugin& m_devicePlugin;

        QModelIndex bottomIndex(const QModelIndex& index) const;

        Device::Node& m_rootNode;

        const iscore::CommandStackFacade& m_cmdQ;

        DeviceExplorerView* m_view {};
};


// Will update the tree and return the messages corresponding to the selected nodes.
ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT State::MessageList getSelectionSnapshot(DeviceExplorerModel& model);

ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceExplorerModel& deviceExplorerFromObject(const QObject&);
ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceExplorerModel* try_deviceExplorerFromObject(const QObject&);
ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceExplorerModel* try_deviceExplorerFromContext(const iscore::DocumentContext& ctx);
ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceExplorerModel& deviceExplorerFromContext(const iscore::DocumentContext& ctx);
}
