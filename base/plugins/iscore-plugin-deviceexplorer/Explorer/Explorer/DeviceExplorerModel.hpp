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

class QMimeData;
class QObject;
namespace iscore {
struct DeviceSettings;
}  // namespace iscore

namespace iscore
{
    class CommandStack;
}

class DeviceDocumentPlugin;
class DeviceEditDialog;
class DeviceExplorerView;

namespace iscore {
struct AddressSettings;
}

class DeviceExplorerModel final : public NodeBasedItemModel
{
        Q_OBJECT

    public:
        enum class Column : int
        {
            Name = 0,
            Value,
            Get,
            Set,
            Min,
            Max,

            Count //column count, always last
        };

        explicit DeviceExplorerModel(
                DeviceDocumentPlugin&,
                QObject* parent = 0);

        ~DeviceExplorerModel();

        iscore::Node& rootNode() override
        { return m_rootNode; }

        const iscore::Node& rootNode() const override
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

        void setCommandQueue(iscore::CommandStack* q);
        iscore::CommandStack& commandStack() const
        { return *m_cmdQ; }

        // Returns the row (useful for undo)
        int addDevice(iscore::Node&& deviceNode);
        int addDevice(const iscore::Node& deviceNode);
        void updateDevice(
                const QString &name,
                const iscore::DeviceSettings& dev);

        void addAddress(
                iscore::Node * parentNode,
                const iscore::AddressSettings& addressSettings,
                int row);
        void updateAddress(
                iscore::Node * node,
                const iscore::AddressSettings& addressSettings);

        void updateValue(iscore::Node* n,
                const iscore::Value& v);

        // Checks if the settings can be added; if not,
        // trigger a dialog to edit them as wanted.
        // Returns true if the device is to be added, false if
        // it should not be added.
        bool checkDeviceInstantiatable(
                iscore::DeviceSettings& n);
        bool tryDeviceInstantiation(
                iscore::DeviceSettings&,
                DeviceEditDialog&);

        bool checkAddressInstantiatable(
                iscore::Node& parent,
                const iscore::AddressSettings& addr);

        bool checkAddressEditable(
                iscore::Node& parent,
                const iscore::AddressSettings& before,
                const iscore::AddressSettings& after);

        int columnCount() const;
        QStringList getColumns() const;
        bool isEmpty() const;
        bool isDevice(QModelIndex index) const;
        bool hasCut() const;

        void debug_printIndexes(const QModelIndexList& indexes);

        int columnCount(const QModelIndex& parent) const override;

        QVariant getData(iscore::NodePath node, Column column, int role);
        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        Qt::ItemFlags flags(const QModelIndex& index) const override;

        bool setData(const QModelIndex& index, const QVariant& value, int role) override;
        bool setHeaderData(int, Qt::Orientation, const QVariant&, int = Qt::EditRole) override;

        void editData(const iscore::NodePath &path, Column column, const iscore::Value& value, int role);

        Qt::DropActions supportedDropActions() const override;
        Qt::DropActions supportedDragActions() const override;
        QStringList mimeTypes() const override;
        QMimeData* mimeData(const QModelIndexList& indexes) const override;
        bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
        bool dropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

        QModelIndex convertPathToIndex(const iscore::NodePath& path);

        QList<iscore::Node*> uniqueSelectedNodes(const QModelIndexList& indexes) const; // Note : filters so that only parents are given.

    protected:
        void debug_printPath(const iscore::NodePath& path);

        typedef QPair<iscore::Node*, bool> CutElt;
        QStack<CutElt> m_cutNodes;
        bool m_lastCutNodeIsCopied;

    private:
        DeviceDocumentPlugin& m_devicePlugin;

        QModelIndex bottomIndex(const QModelIndex& index) const;

        iscore::Node& m_rootNode;

        iscore::CommandStack* m_cmdQ;

        DeviceExplorerView* m_view {};
};


// Will update the tree and return the messages corresponding to the selected nodes.
iscore::MessageList getSelectionSnapshot(DeviceExplorerModel& model);

DeviceExplorerModel& deviceExplorerFromObject(const QObject&);
DeviceExplorerModel* try_deviceExplorerFromObject(const QObject&);
