#pragma once

#include <QAbstractItemModel>
#include <QStack>

namespace iscore
{
    class CommandStack;
}
namespace DeviceExplorer {
    namespace Command {
        class Move;
        class Insert;
        class Remove;
        class Copy;
        class Cut;
        class Paste;
    }
}

class Node;
class DeviceExplorerView;


class DeviceExplorerModel : public QAbstractItemModel
{
        Q_OBJECT

        friend class DeviceExplorer::Command::Move;
        friend class DeviceExplorer::Command::Insert;
        friend class DeviceExplorer::Command::Remove;
        friend class DeviceExplorer::Command::Copy;
        friend class DeviceExplorer::Command::Cut;
        friend class DeviceExplorer::Command::Paste;

    public:

        enum Insert {AsSibling, AsChild};

        explicit DeviceExplorerModel(QObject* parent = 0);
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
                        const QList<QString>& addressSettings);

        void removeLeave(Node *node);


        int columnCount() const;
        QStringList getColumns() const;
        bool isEmpty() const;
        bool isDevice(QModelIndex index) const;
        bool hasCut() const;

        void debug_printIndexes(const QModelIndexList& indexes);

        bool getTreeData(const QModelIndex& index, QByteArray& data) const;
        bool insertTreeData(const QModelIndex& parent, int row, const QByteArray& data);


        /*
          The following methods return a QModelIndex that can be used as the new selection.
         */

        QModelIndex copy(const QModelIndex& index);
        QModelIndex cut(const QModelIndex& index);
        QModelIndex paste(const QModelIndex& index);

        //Move up node at @a index among siblings
        QModelIndex moveUp(const QModelIndex& index);

        //Move down node at @a index among siblings
        QModelIndex moveDown(const QModelIndex& index);

        //Move node at @a index at its parent level,
        //i.e., making node a child of its grandparent
        // and the next sibling of its parent.
        QModelIndex promote(const QModelIndex& index);

        //Move node at @a index as child of its up sibling
        QModelIndex demote(const QModelIndex& index);




        QModelIndex index(int row, int column, const QModelIndex& parent) const override;
        QModelIndex parent(const QModelIndex& child) const override;
        int rowCount(const QModelIndex& parent) const override;
        int columnCount(const QModelIndex& parent) const override;

        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

        Qt::ItemFlags flags(const QModelIndex& index) const override;

        bool setData(const QModelIndex& index, const QVariant& value, int role) override;
        bool setHeaderData(int, Qt::Orientation, const QVariant&, int = Qt::EditRole) override;

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

        typedef QList<int> Path;
        Node* nodeFromModelIndex(const QModelIndex& index) const;
        Path pathFromNode(Node &node);
        Node *pathToNode(const Path& path);

    protected:
        Path pathFromIndex(const QModelIndex& index);
        QModelIndex pathToIndex(const Path& path);

        static void serializePath(QDataStream& d, const DeviceExplorerModel::Path& p);
        static void deserializePath(QDataStream& d, DeviceExplorerModel::Path& p);

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

        DeviceExplorerModel::Result cut_aux(const QModelIndex& index);
        DeviceExplorerModel::Result paste_aux(const QModelIndex& index, bool after);
        DeviceExplorerModel::Result pasteBefore_aux(const QModelIndex& index);
        DeviceExplorerModel::Result pasteAfter_aux(const QModelIndex& index);


        void debug_printPath(const Path& path);

        void setCachedResult(Result r);

    private:
        Node* createRootNode() const;

        void setColumnValue(Node* node, const QVariant& v, int col);
        QModelIndex bottomIndex(const QModelIndex& index) const;
        QModelIndex moveChildAmongSiblings(Node* parent, int oldRow, int newRow);


        Node* m_rootNode;

        typedef QPair<Node*, bool> CutElt;
        QStack<CutElt> m_cutNodes;
        bool m_lastCutNodeIsCopied;

        iscore::CommandStack* m_cmdQ;
        Result m_cachedResult;

        DeviceExplorerView* m_view {};
};

