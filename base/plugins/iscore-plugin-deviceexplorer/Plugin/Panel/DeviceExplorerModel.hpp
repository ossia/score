#pragma once

#include <QAbstractItemModel>
#include <iscore/serialization/VisitorInterface.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <DeviceExplorer/ItemModels/NodeBasedItemModel.hpp>


#include "Result.hpp"

#include <QStack>

namespace iscore
{
    class CommandStack;
}
namespace DeviceExplorer {
    namespace Command {
        class Insert;
        class Cut;
        class Paste;
    }
}

class DeviceExplorerView;
class DeviceExplorerCommandCreator;
class DeviceDocumentPlugin;
class DeviceEditDialog;
namespace iscore {
struct AddressSettings;
}

class DeviceExplorerModel : public NodeBasedItemModel
{
        Q_OBJECT

        friend class DeviceExplorer::Command::Insert;
        friend class DeviceExplorer::Command::Cut;
        friend class DeviceExplorer::Command::Paste;
        friend class DeviceExplorerCommandCreator;

    public:
        enum class Column : int
        {
            Name = 0,
            Value,
            IOType,
            Min,
            Max,

            Count //column count, always last
        };

        explicit DeviceExplorerModel(DeviceDocumentPlugin*, QObject* parent = 0);

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

        // Returns the row (useful for undo)
        int addDevice(iscore::Node&& deviceNode);
        int addDevice(const iscore::Node& deviceNode);
        void updateDevice(
                const QString &name,
                const iscore::DeviceSettings& dev);

        void addAddress(
                iscore::Node * parentNode,
                const iscore::AddressSettings& addressSettings);
        void updateAddress(
                iscore::Node * node,
                const iscore::AddressSettings& addressSettings);

        void updateValue(iscore::Node* n,
                const iscore::Value& v);

        void removeNode(iscore::Node::const_iterator node);

        // Checks if the settings can be added; if not,
        // trigger a dialog to edit them as wanted.
        // Returns true if the device is to be added, false if
        // it should not be added.
        bool checkDeviceInstantiatable(iscore::DeviceSettings& n);
        bool tryDeviceInstantiation(iscore::DeviceSettings&,
                                    DeviceEditDialog&);

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

        void editData(const iscore::NodePath &path, Column column, const QVariant& value, int role);

        virtual bool moveRows(const QModelIndex& srcParent, int srcRow, int count, const QModelIndex& dstParent, int dstChild) override;


        virtual Qt::DropActions supportedDropActions() const override;
        virtual Qt::DropActions supportedDragActions() const override;
        virtual QStringList mimeTypes() const override;
        virtual QMimeData* mimeData(const QModelIndexList& indexes) const override;
        virtual bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
        virtual bool dropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

        QModelIndex convertPathToIndex(const iscore::NodePath& path);

        DeviceExplorerCommandCreator* cmdCreator();
        void setCachedResult(DeviceExplorer::Result r);

    protected:
        DeviceExplorer::Result cut_aux(const QModelIndex& index);
        DeviceExplorer::Result paste_aux(const QModelIndex& index, bool after);
        DeviceExplorer::Result pasteBefore_aux(const QModelIndex& index);
        DeviceExplorer::Result pasteAfter_aux(const QModelIndex& index);

        void debug_printPath(const iscore::NodePath& path);

        typedef QPair<iscore::Node*, bool> CutElt;
        QStack<CutElt> m_cutNodes;
        bool m_lastCutNodeIsCopied;

    private:
        DeviceDocumentPlugin* m_devicePlugin{};

        QModelIndex bottomIndex(const QModelIndex& index) const;

        iscore::Node& m_rootNode;

        iscore::CommandStack* m_cmdQ;

        DeviceExplorerView* m_view {};

        DeviceExplorerCommandCreator* m_cmdCreator;
};
