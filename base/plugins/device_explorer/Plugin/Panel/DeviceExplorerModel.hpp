#pragma once

#include <QAbstractItemModel>
#include <iscore/command/OngoingCommandManager.hpp>
#include <DeviceExplorer/NodePath.hpp>

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
    struct Result
    {
        Result(bool ok_ = true, const QModelIndex& index_ = QModelIndex())
            : ok(ok_), index(index_)
        {}

        Result(const QModelIndex& index_)
            : ok(true), index(index_)
        {}

        //Type-cast operators
        operator bool() const
        {
            return ok;
        }
        operator QModelIndex() const
        {
            return index;
        }

        bool ok;
        QModelIndex index;
    };
}

class Node;
class DeviceExplorerView;
class DeviceExplorerCommandCreator;
struct AddressSettings;

class DeviceExplorerModel : public QAbstractItemModel
{
        Q_OBJECT

        friend class DeviceExplorer::Command::Insert;
        friend class DeviceExplorer::Command::Cut;
        friend class DeviceExplorer::Command::Paste;
        friend class DeviceExplorerCommandCreator;

    public:

        enum class Insert {AsSibling, AsChild};

        explicit DeviceExplorerModel(QObject* parent = 0);
        DeviceExplorerModel(const VisitorVariant& data, QObject* parent);

        ~DeviceExplorerModel();

        Node* rootNode() const
        {
            return m_rootNode;
        }

        void setView(DeviceExplorerView* v)
        {
            m_view = v;
        }

        QModelIndexList selectedIndexes() const;

        void setCommandQueue(iscore::CommandStack* q);
        bool load(const QString& filename);

        // Returns the row (useful for undo)
        int addDevice(Node* deviceNode);
        Node *addAddress(Node * parentNode,
                        const AddressSettings& addressSettings);
        void addAddress(Node* parentNode, Node* node);

        void removeNode(Node *node);


        int columnCount() const;
        QStringList getColumns() const;
        bool isEmpty() const;
        bool isDevice(QModelIndex index) const;
        bool hasCut() const;

        void debug_printIndexes(const QModelIndexList& indexes);

        bool getTreeData(const QModelIndex& index, QByteArray& data) const;
        bool insertTreeData(const QModelIndex& parent, int row, const QByteArray& data);


        QModelIndex index(int row, int column, const QModelIndex& parent) const override;
        QModelIndex parent(const QModelIndex& child) const override;
        int rowCount(const QModelIndex& parent) const override;
        int columnCount(const QModelIndex& parent) const override;

        QVariant getData(Path node, int column, int role);
        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        Qt::ItemFlags flags(const QModelIndex& index) const override;

        bool setData(const QModelIndex& index, const QVariant& value, int role) override;
        bool setHeaderData(int, Qt::Orientation, const QVariant&, int = Qt::EditRole) override;

        void editData(const Path &path, int column, const QVariant& value, int role);

        virtual bool moveRows(const QModelIndex& srcParent, int srcRow, int count, const QModelIndex& dstParent, int dstChild) override;
        bool undoMoveRows(const QModelIndex& srcParent, int srcRow, int count, const QModelIndex& dstParent, int dstRow);


        //virtual bool insertRows(int row, int count, const QModelIndex &parent) override;
        virtual bool removeRows(int row, int count, const QModelIndex& parent) override;
        virtual Qt::DropActions supportedDropActions() const override;
        virtual Qt::DropActions supportedDragActions() const override;
        virtual QStringList mimeTypes() const override;
        virtual QMimeData* mimeData(const QModelIndexList& indexes) const override;
        virtual bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
        virtual bool dropMimeData(const QMimeData* mimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;

        int getIOTypeColumn() const;
        int getNameColumn() const;

//        typedef QList<int> Path;
        Node* nodeFromModelIndex(const QModelIndex& index) const;
        QModelIndex pathToIndex(const Path& path);

        DeviceExplorerCommandCreator* cmdCreator();
        void setCachedResult(DeviceExplorer::Result r);

    protected:

        DeviceExplorer::Result cut_aux(const QModelIndex& index);
        DeviceExplorer::Result paste_aux(const QModelIndex& index, bool after);
        DeviceExplorer::Result pasteBefore_aux(const QModelIndex& index);
        DeviceExplorer::Result pasteAfter_aux(const QModelIndex& index);

        void debug_printPath(const Path& path);

        typedef QPair<Node*, bool> CutElt;
        QStack<CutElt> m_cutNodes;
        bool m_lastCutNodeIsCopied;

    private:
        Node* createRootNode() const;

        void setColumnValue(Node* node, const QVariant& v, int col);
        QModelIndex bottomIndex(const QModelIndex& index) const;

        Node* m_rootNode;

        iscore::CommandStack* m_cmdQ;

        DeviceExplorerView* m_view {};

        DeviceExplorerCommandCreator* m_cmdCreator;
};

