#include "DeviceExplorerModel.hpp"
#include "DeviceExplorerView.hpp"
#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorerCommandCreator.hpp"

#include <iostream>

#include <QMimeData>

#include "Commands/Move.hpp"
#include "Commands/Insert.hpp"
#include "Commands/Cut.hpp"
#include "Commands/Paste.hpp"
#include "Commands/EditData.hpp"

using namespace DeviceExplorer::Command;

enum
{
    NAME_COLUMN = 0,
    VALUE_COLUMN,
    IOTYPE_COLUMN,
    MIN_COLUMN,
    MAX_COLUMN,
    PRIORITY_COLUMN,

    COLUMN_COUNT //always last
};


static QString HEADERS[COLUMN_COUNT];


class BinaryWriter
{
        QDataStream& m_stream;

    public:
        BinaryWriter(QDataStream& stream)
            : m_stream(stream)
        {

        }

        void write(const Node* n)
        {
            Q_ASSERT(n);

            m_stream << n->name();
            //TODO: type !
            m_stream << n->value();
            m_stream << (qint32) n->ioType();
            m_stream << n->minValue();
            m_stream << n->maxValue();
            m_stream << (quint32) n->priority();
            //TODO: cropMode, tags, ...

            m_stream << (quint32) n->childCount();
            foreach(Node * child, n->children())
            write(child);

        }

};

class BinaryReader
{
        QDataStream& m_stream;

    public:
        BinaryReader(QDataStream& stream)
            : m_stream(stream)
        {
        }

        void read(Node* parent)
        {
            Q_ASSERT(parent);

            QString name;
            m_stream >> name;

            Node* n = new Node(name, parent);

            //TODO: type !
            QString value;
            m_stream >> value;
            qint32 ioType;
            m_stream >> ioType;
            float minValue, maxValue;
            m_stream >> minValue;
            m_stream >> maxValue;
            quint32 priority;
            m_stream >> priority;
            //TODO: cropMode, tags, ...

            n->setValue(value);
            n->setIOType(static_cast<Node::IOType>(ioType));
            n->setMinValue(minValue);
            n->setMaxValue(maxValue);
            n->setPriority(priority);

            quint32 childCount;
            m_stream >> childCount;

            for(quint32 i = 0; i < childCount; ++i)
            {
                read(n);
            }

        }
};



DeviceExplorerModel::DeviceExplorerModel(QObject* parent)
    : QAbstractItemModel(parent),
      m_lastCutNodeIsCopied(false),
      m_rootNode(nullptr),
      m_cmdQ(nullptr)
{
    this->setObjectName("DeviceExplorerModel");

    HEADERS[NAME_COLUMN] = tr("Address");
    HEADERS[VALUE_COLUMN] = tr("Value");
    HEADERS[IOTYPE_COLUMN] = tr("I/O");
    HEADERS[MIN_COLUMN] = tr("min");
    HEADERS[MAX_COLUMN] = tr("max");
    HEADERS[PRIORITY_COLUMN] = tr("priority");

    m_cmdCreator = new DeviceExplorerCommandCreator{this};

}

DeviceExplorerModel::DeviceExplorerModel(const VisitorVariant& vis, QObject* parent)
    : QAbstractItemModel(parent),
      m_lastCutNodeIsCopied(false),
      m_rootNode(nullptr),
      m_cmdQ(nullptr)
{
    this->setObjectName("DeviceExplorerModel");

    HEADERS[NAME_COLUMN] = tr("Address");
    HEADERS[VALUE_COLUMN] = tr("Value");
    HEADERS[IOTYPE_COLUMN] = tr("I/O");
    HEADERS[MIN_COLUMN] = tr("min");
    HEADERS[MAX_COLUMN] = tr("max");
    HEADERS[PRIORITY_COLUMN] = tr("priority");


    beginResetModel();

    m_rootNode = new Node;
    if(vis.identifier == DataStream::type())
    {
        auto& des = static_cast<DataStream::Deserializer&>(vis.visitor);
        bool b;
        des.m_stream >> b;
        if(b)
            des.writeTo(*m_rootNode);
    }
    else if(vis.identifier == JSON::type())
    {
        auto& des = static_cast<JSON::Deserializer&>(vis.visitor);
        des.writeTo(*m_rootNode);
    }

    endResetModel();


}

DeviceExplorerModel::~DeviceExplorerModel()
{
    delete m_rootNode;

    for(QStack<CutElt>::iterator it = m_cutNodes.begin();
            it != m_cutNodes.end(); ++it)
    {
        delete it->first;
    }

}

QModelIndexList DeviceExplorerModel::selectedIndexes() const
{
    return m_view->selectedIndexes();
}

//TODO:REMOVE

#include <QFile>

//TODO:REMOVE
static
Node*
readNode(QTextStream& in, Node* parent)
{
    //const int nbValues = 13;
    QString name;
    in >> name;

    if(name.isEmpty())
    {
        return NULL;
    }

    if(in.status() != QTextStream::Ok)
    {
        std::cerr << "status not ok (2)\n";
        exit(10);
    }

    if(in.atEnd())
    {
        return 0;
    }

    static const QString INVALID = "-_-";

    if(name == INVALID)
    {
        return 0;
    }

    Node* node = new Node(name, parent);

    QString value;
    in >> value;

    if(value != INVALID)
    {
        node->setValue(value);
    }

    QString startAssignation;
    in >> startAssignation;

    QString start;
    in >> start;

    QString interpolation;
    in >> interpolation;

    QString endAssignation;
    in >> endAssignation;

    QString end;
    in >> end;

    QString redundancy;
    in >> redundancy;

    QString sr;
    in >> sr;

    QString iotype;
    in >> iotype;

    if(iotype != INVALID)
    {
        if(iotype == "->")
        {
            node->setIOType(Node::In);
        }
        else if(iotype == "<-")
        {
            node->setIOType(Node::Out);
        }
        else if(iotype == "<->")
        {
            node->setIOType(Node::InOut);
        }
    }

    QString minB;
    in >> minB;

    if(minB != INVALID)
    {
        node->setMinValue(minB.toFloat());
    }

    QString maxB;
    in >> maxB;

    if(maxB != INVALID)
    {
        node->setMaxValue(maxB.toFloat());
    }

    QString priority;
    in >> priority;

    if(priority != INVALID)
    {
        node->setPriority(priority.toInt());
    }

    int nbChildren = 0;
    in >> nbChildren;

    //node->_children.reserve(nbChildren);
    for(int i = 0; i < nbChildren; ++i)
    {
        readNode(in, node);  //add read child to node.
        //Node *n = readNode(in, node);
        //if (n != nullptr)
        // node->addChild(n);
    }

    return node;
}

bool
DeviceExplorerModel::load(const QString& filename)
{
    QFile file(filename);

    if(! file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QTextStream in(&file);

    Node* node = readNode(in, nullptr);

    if(node == nullptr)
    {
        std::cerr << "Error: unabel to read: " << filename.toStdString() << "\n";
        return false;
    }


    beginResetModel();

    delete m_rootNode;
    m_rootNode = createRootNode();
    m_rootNode->addChild(node);

    endResetModel();


    return true;
}

void
DeviceExplorerModel::setCommandQueue(iscore::CommandStack* q)
{
    m_cmdQ = q;
    m_cmdCreator->setCommandQueue(q);
}

QStringList
DeviceExplorerModel::getColumns() const
{
    QStringList l;

    for(int i = 0; i < COLUMN_COUNT; ++i)
    {
        l.append(HEADERS[i]);
    }

    return l;
}

int DeviceExplorerModel::addDevice(Node* deviceNode)
{

    if(m_rootNode == nullptr)
    {
        m_rootNode = createRootNode();
    }

    Q_ASSERT(m_rootNode);


    //we always insert as last child of m_rootNode
    //TODO: insert after current selected device ???

    int row = m_rootNode->childCount();
    QModelIndex parent; //invalid
    beginInsertRows(parent, row, row);
    deviceNode->setParent(m_rootNode);

    endInsertRows();

    return row;
}

Node* DeviceExplorerModel::addAddress(Node* parentNode, const AddressSettings &addressSettings)
{
    Q_ASSERT(parentNode);
    Q_ASSERT(parentNode != m_rootNode);

    int row = parentNode->childCount(); //insert as last child

    Node* grandparent = parentNode->parent();
    Q_ASSERT(grandparent);
    int rowParent = grandparent->indexOfChild(parentNode);
    QModelIndex parentIndex = createIndex(rowParent, 0, parentNode);

    beginInsertRows(parentIndex, row, row);

    auto node = makeNode(addressSettings);
    parentNode->insertChild(row, node);

    endInsertRows();

    return node;
}

void DeviceExplorerModel::addAddress(Node *parentNode, Node *node)
{
    Q_ASSERT(parentNode);
    Q_ASSERT(parentNode != m_rootNode);

    int row = parentNode->childCount(); //insert as last child

    Node* grandparent = parentNode->parent();
    Q_ASSERT(grandparent);
    int rowParent = grandparent->indexOfChild(parentNode);
    QModelIndex parentIndex = createIndex(rowParent, 0, parentNode);

    beginInsertRows(parentIndex, row, row);

    parentNode->insertChild(row, node);

    endInsertRows();
}

void DeviceExplorerModel::removeNode(Node* node)
{
    Q_ASSERT(node);
    Q_ASSERT(node != m_rootNode);

    Node* parent = node->parent();

    Q_ASSERT(parent != m_rootNode);
    Node* grandparent = parent->parent();
    Q_ASSERT(grandparent);
    int rowParent = grandparent->indexOfChild(parent);
    QModelIndex parentIndex = createIndex(rowParent, 0, parent);

    int row = parent->indexOfChild(node);

    beginRemoveRows(parentIndex, row, row);
    parent->removeChild(node);
    endRemoveRows();
}


QModelIndex
DeviceExplorerModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!m_rootNode || row < 0 || column < 0 || column >= COLUMN_COUNT)
    {
        return QModelIndex();
    }

    Node* parentNode = nodeFromModelIndex(parent);
    assert(parentNode);
    Node* childNode = parentNode->childAt(row);  //value() return 0 if out of bounds

    if(! childNode)
    {
        return QModelIndex();
    }

    return createIndex(row, column, childNode);
}

Node*
DeviceExplorerModel::nodeFromModelIndex(const QModelIndex& index) const
{
    if(index.isValid())
    {
        return static_cast<Node*>(index.internalPointer());
    }
    else
    {
        return m_rootNode;
    }
}

QModelIndex
DeviceExplorerModel::parent(const QModelIndex& child) const
{
    Node* node = nodeFromModelIndex(child);

    if(! node)
    {
        return QModelIndex();
    }

    Node* parentNode = node->parent();

    if(! parentNode)
    {
        return QModelIndex();
    }

    Node* grandparentNode = parentNode->parent();

    if(! grandparentNode)
    {
        return QModelIndex();
    }

    const int rowParent = grandparentNode->indexOfChild(parentNode);  //(return -1 if not found)
    assert(rowParent != -1);
    return createIndex(rowParent, 0, parentNode);
}

int
DeviceExplorerModel::rowCount(const QModelIndex& parent) const
{
    if(parent.column() > 0)
    {
        return 0;
    }

    Node* parentNode = nodeFromModelIndex(parent);

    if(! parentNode)
    {
        return 0;
    }

    return parentNode->childCount();
}


int
DeviceExplorerModel::columnCount() const
{
    return COLUMN_COUNT;
}

int
DeviceExplorerModel::columnCount(const QModelIndex& /*parent*/) const
{
    return COLUMN_COUNT;
}

QVariant DeviceExplorerModel::getData(Path node, int column, int role)
{
    QModelIndex index = createIndex(pathToIndex(node).row(), column, node.toNode(rootNode())->parent());
    return data(index, role);
}

// must return an invalid QVariant for cases not handled
QVariant
DeviceExplorerModel::data(const QModelIndex& index, int role) const
{
    //if (role != Qt::DisplayRole)
    //return QVariant();

    const int col = index.column();

    if(col < 0 || col >= COLUMN_COUNT)
    {
        return QVariant();
    }

    Node* node = nodeFromModelIndex(index);

    if(! node)
    {
        return QVariant();
    }

    switch(col)
    {
        case NAME_COLUMN:
        {
            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                return node->name();
            }
            else if(role == Qt::FontRole)
            {
                const Node::IOType ioType = node->ioType();

                if(ioType == Node::In || ioType == Node::Out)
                {
                    QFont f; // = QAbstractItemModel::data(index, role); //B: how to get current font ?
                    f.setItalic(true);
                    return f;
                }
            }
            else if(role == Qt::ForegroundRole)
            {
                const Node::IOType ioType = node->ioType();

                if(ioType == Node::In || ioType == Node::Out)
                {
                    return QBrush(Qt::black);
                }
            }

        }
        break;

        case VALUE_COLUMN:
        {
            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                return node->value();
            }
            else if(role == Qt::ForegroundRole)
            {
                const Node::IOType ioType = node->ioType();

                if(ioType == Node::In || ioType == Node::Out)
                {
                    return QBrush(Qt::black);
                }
            }
        }
        break;

        case IOTYPE_COLUMN:
        {
            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                switch(node->ioType())
                {
                    case Node::In:
                        return QString("<-");
                        break;

                    case Node::Out:
                        return QString("->");
                        break;

                    case Node::InOut:
                        return QString("<->");
                        break;

                    default:
                        return QVariant();
                        break;
                }
            }
        }
        break;

        case MIN_COLUMN:
        {
            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                const float minV = node->minValue();

                if(minV != Node::INVALID_FLOAT)
                {
                    return minV;
                }

                return QString();
            }
        }
        break;

        case MAX_COLUMN:
        {
            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                const float maxV = node->maxValue();

                if(maxV != Node::INVALID_FLOAT)
                {
                    return maxV;
                }

                return QString();
            }
        }
        break;

        case PRIORITY_COLUMN:
        {
            if(role == Qt::DisplayRole || role == Qt::EditRole)
            {
                return node->priority();
            }
        }
        break;

        default :
            assert(false);
            return QString();
    }



    return QVariant();
}

void
DeviceExplorerModel::setColumnValue(Node* node, const QVariant& v, int col)
{
    if(col < 0 || col >= COLUMN_COUNT)
    {
        return;
    }

    assert(node);

    switch(col)
    {
        case NAME_COLUMN:
        {
            node->setName(v.toString());
        }
        break;

        case VALUE_COLUMN:
        {
            node->setValue(v.toString());
        }
        break;

        case IOTYPE_COLUMN:
        {
            //node->setType(v.toString());
        }
        break;

        case MIN_COLUMN:
        {
            node->setMinValue(v.toFloat());   //TODO:change ! no necessarily a float !
        }
        break;

        case MAX_COLUMN:
        {
            node->setMaxValue(v.toFloat());   //TODO:change ! no necessarily a float !
        }
        break;

        case PRIORITY_COLUMN:
        {
            node->setPriority(v.toUInt());
        }
        break;

        default :
            assert(false);
    }

}


QVariant
DeviceExplorerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if(section >= 0 && section < COLUMN_COUNT)
        {
            return HEADERS[section];
        }
    }

    return QVariant();
}

Qt::ItemFlags
DeviceExplorerModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = Qt::ItemIsEnabled;
    //by default QAbstractItemModel::flags(index); returns Qt::ItemIsEnabled | Qt::ItemIsSelectable

    if(index.isValid())
    {
        Node* n = nodeFromModelIndex(index);
        assert(n);

        if(n->isSelectable())
        {
            f |= Qt::ItemIsSelectable;
        }

        //we allow drag'n drop only from the name column
        if(index.column() == NAME_COLUMN)
        {
            f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
        }


        if(n->isEditable())
        {
            //std::cerr<<"DeviceExplorerModel::flags "<<n->name().toStdString()<<" ioType="<<(int)n->ioType()<<" Qt::ItemIsEditable\n";

            f |= Qt::ItemIsEditable;
        }

    }
    else
    {
        //to be able to drop even where there is nothing
        f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    }

    return f;
}

/*
  return false if no change was made.
  emit dataChanged() & return true if a change is made.
*/
bool
DeviceExplorerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(! index.isValid())
    {
        return false;
    }

    if(! nodeFromModelIndex(index))
    {
        return false;
    }

    bool changed = false;

    //TODO: check what's editable or not !!!

    if(role == Qt::EditRole)
    {
        if(index.column() == NAME_COLUMN)
        {
            const QString s = value.toString();

            if(! s.isEmpty())
            {
                m_cmdQ->redoAndPush(new EditData{iscore::IDocument::path(this), Path{index}, index.column(), value, role});
                changed = true;
            }
        }

        if(index.column() == IOTYPE_COLUMN)
        {
            m_cmdQ->redoAndPush(new EditData{iscore::IDocument::path(this), Path{index}, index.column(), value, role});
            changed = true;
        }
    }

    return changed; //false;
}

bool
DeviceExplorerModel::setHeaderData(int, Qt::Orientation, const QVariant&, int)
{
    return false; //we prevent editing the (column) headers
}

void DeviceExplorerModel::editData(const Path &path, int column, const QVariant &value, int role)
{
    //TODO: check what's editable or not !!!
    QModelIndex nodeIndex = pathToIndex(path);
    Node* node = nodeFromModelIndex(nodeIndex);

    QModelIndex index = createIndex(nodeIndex.row(), column, node->parent());

    QModelIndex changedTopLeft = index;
    QModelIndex changedBottomRight = index;

    if(role == Qt::EditRole)
    {
        if(index.column() == NAME_COLUMN)
        {
            const QString s = value.toString();

            if(! s.isEmpty())
            {
                node->setName(s);
            }
        }

        if(index.column() == IOTYPE_COLUMN)
        {
            const QString iotype = value.toString();

            if(iotype == "->")
            {
                node->setIOType(Node::In);
            }
            else if(iotype == "<-")
            {
                node->setIOType(Node::Out);
            }
            else if(iotype == "<->")
            {
                node->setIOType(Node::InOut);
            }
        }
    }

    //NON ! emitDatachanged devrait être fait pour tous les indices des fils !!!

    {
        Node* n1 = nodeFromModelIndex(changedTopLeft);
        Node* n2 = nodeFromModelIndex(changedBottomRight);
        std::cerr << "DeviceExplorerModel::setData i1=(" << n1->name().toStdString() << ", " << changedTopLeft.row() << ", " << changedTopLeft.column() << ") i2=(" << n2->name().toStdString() << ", " << changedBottomRight.row() << ", " << changedBottomRight.column() << ")\n";
    }

    emit dataChanged(changedTopLeft, changedBottomRight);
}


QModelIndex
DeviceExplorerModel::bottomIndex(const QModelIndex& index) const
{
    Node* node = nodeFromModelIndex(index);

    if(! node)
    {
        return index;
    }

    if(! node->hasChildren())
    {
        return index;
    }

    return bottomIndex(createIndex(node->childCount() - 1, index.column(), node->childAt(node->childCount() - 1)));
}

Node*
DeviceExplorerModel::createRootNode() const
{
    return new Node("Invisible root", nullptr);
}


//this method is called (behind the scenes) when there is a drag and drop to delete the original dragged rows once they have been dropped (dropped rows are inserted using insertRows)
bool
DeviceExplorerModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if(! m_rootNode)
    {
        return false;
    }

    Node* parentNode = parent.isValid() ? nodeFromModelIndex(parent) : m_rootNode;
    beginRemoveRows(parent, row, row + count - 1);

    for(int i = 0; i < count; ++i)
    {
        Node* n = parentNode->takeChild(row);
        delete n;
    }

    endRemoveRows();

    return true;
}

int
DeviceExplorerModel::getIOTypeColumn() const
{
    return IOTYPE_COLUMN;
}

int
DeviceExplorerModel::getNameColumn() const
{
    return NAME_COLUMN;
}


bool
DeviceExplorerModel::isDevice(QModelIndex index) const
{
    if(!index.isValid())
    {
        return true;
    }

    Node* n = nodeFromModelIndex(index);
    Q_ASSERT(n);

    Node* parent = n->parent();
    Q_ASSERT(parent);

    return parent == m_rootNode;
}

bool
DeviceExplorerModel::isEmpty() const
{
    return (m_rootNode == nullptr || m_rootNode->childCount() == 0);
}

bool
DeviceExplorerModel::hasCut() const
{
    return (! m_cutNodes.isEmpty());
}

/*
  Copy|Cut / Paste behavior

  We do not do : serialize for Copy, or serialize+remove for Cut
  but we keep the nodes alive.
  We think that deleting nodes may be costly.

  We store cut nodes in m_cutNodes.

  We need to store several cut nodes to be able to do several undos
  on a CutCommands.
  (Merge CutCommands together would not be a solution as they can be interlaced with other commands).

  BUT IT MEANS that cut nodes are never deleted during the lifetime of the Model !

*/

DeviceExplorerModel::Result
DeviceExplorerModel::cut_aux(const QModelIndex& index)
{
    if(!index.isValid())
    {
        return Result(false, index);
    }


    Node* cutNode = nodeFromModelIndex(index);
    Q_ASSERT(cutNode);

    const bool cutNodeIsDevice = cutNode->isDevice();


    if(! m_cutNodes.isEmpty() && m_lastCutNodeIsCopied)
    {
        //necessary to avoid that several successive copies fill m_cutNodes (copy is not a command)
        Node* prevCopiedNode = m_cutNodes.pop().first;
        delete prevCopiedNode;
    }

    m_cutNodes.push(CutElt(cutNode, cutNodeIsDevice));
    m_lastCutNodeIsCopied = false;

    Node* parent = cutNode->parent();
    Q_ASSERT(parent);

    int row = parent->indexOfChild(cutNode);
    Q_ASSERT(row == index.row());

    beginRemoveRows(index.parent(), row, row);

    #ifndef QT_NO_DEBUG
    Node* child =
    #endif
        parent->takeChild(row);
    Q_ASSERT(child == cutNode);

    endRemoveRows();

    //TODO: we should emit a signal to indicate that a paste is now possible !?!

    if(row > 0)
    {
        --row;
        return createIndex(row, 0, parent->childAt(row));
    }

    if(parent != m_rootNode)
    {
        Node* grandParent = parent->parent();
        Q_ASSERT(grandParent);
        return createIndex(grandParent->indexOfChild(parent), 0, parent);
    }

    return QModelIndex();
}


/*
  Paste behavior.

  We always paste as a sibling of the selected index.
  We provide two methods pasteBefore() & pasteAfter().

  pasteAfter() inserts the pasted node after the selected node in its parent list.
  Thus it means that we can never paste the item as first child.

  pasteBefore() inserts the pasted node before the selected node in its parent list.
  Thus we can paste as first child but we can not paste as last child.

*/

DeviceExplorerModel::Result
DeviceExplorerModel::paste_aux(const QModelIndex& index, bool after)
{
    if(m_cutNodes.isEmpty())
    {
        return Result(false, index);
    }

    if(! index.isValid() && ! m_cutNodes.top().second)  //we can not pass addresses at top level
    {
        return Result(false, index);
    }


    //REM: we always paste as sibling


    Node* parent = nullptr;
    int row = 0;
    QModelIndex parentIndex;

    const bool cutNodeIsDevice = m_cutNodes.top().second;
    Node* cutNode = m_cutNodes.top().first;
    m_cutNodes.pop();

    if(index.isValid())
    {
        Node* n = nodeFromModelIndex(index);
        Q_ASSERT(n);

        parent = n->parent();
        Q_ASSERT(parent);

        parentIndex = index.parent();

        if(cutNodeIsDevice)
        {
            //we can only paste devices at the top-level
            while(parent != m_rootNode)
            {
                Q_ASSERT(parent);
                n = parent;
                parent = parent->parent();
            }

            Q_ASSERT(parent->indexOfChild(n) != -1);

            parentIndex = QModelIndex(); //invalid on purpose

        }

        row = parent->indexOfChild(n) + (after ? 1 : 0);

    }
    else
    {
        Q_ASSERT(! index.isValid() && cutNodeIsDevice);
        parent = m_rootNode;
        row = m_rootNode->childCount();
        parentIndex = QModelIndex(); //invalid on purpose
    }

    Q_ASSERT(parent);

    Q_ASSERT(cutNode);

    beginInsertRows(parentIndex, row, row);

    parent->insertChild(row, cutNode);

    Node* child = cutNode;

    endInsertRows();

    return createIndex(row, 0, child);
}

DeviceExplorerModel::Result
DeviceExplorerModel::pasteAfter_aux(const QModelIndex& index)
{
    return paste_aux(index, true);
}

DeviceExplorerModel::Result
DeviceExplorerModel::pasteBefore_aux(const QModelIndex& index)
{
    return paste_aux(index, false);
}

bool
DeviceExplorerModel::moveRows(const QModelIndex& srcParentIndex, int srcRow, int count,
                              const QModelIndex& dstParentIndex, int dstRow)
{
    if(!srcParentIndex.isValid() || !dstParentIndex.isValid())
    {
        return false;
    }

    if(srcParentIndex == dstParentIndex && (srcRow <= dstRow && dstRow <= srcRow + count - 1))
    {
        return false;
    }

    Node* srcParent = nodeFromModelIndex(srcParentIndex);
    Q_ASSERT(srcParent);

    if(srcRow + count > srcParent->childCount())
    {
        return false;
    }


    beginMoveRows(srcParentIndex, srcRow, srcRow + count - 1, dstParentIndex, dstRow);

    if(srcParentIndex == dstParentIndex)
    {
        //move up or down inside the same parent

        Node* parent = srcParent;
        Q_ASSERT(parent);

        if(srcRow > dstRow)
        {
            for(int i = 0; i < count; ++i)
            {
                Node* n = parent->takeChild(srcRow + i);
                parent->insertChild(dstRow + i, n);
            }
        }
        else
        {
            Q_ASSERT(srcRow < dstRow);

            for(int i = 0; i < count; ++i)
            {
                Node* n = parent->takeChild(srcRow);
                parent->insertChild(dstRow - 1, n);
            }
        }

    }
    else
    {
        //different parents

        Node* dstParent = nodeFromModelIndex(dstParentIndex);
        Q_ASSERT(dstParent);
        Q_ASSERT(dstParent != srcParent);

        for(int i = 0; i < count; ++i)
        {
            Node* n = srcParent->takeChild(srcRow);
            dstParent->insertChild(dstRow + i, n);
        }

    }

    endMoveRows();

    return true;
}

bool
DeviceExplorerModel::undoMoveRows(const QModelIndex& srcParentIndex, int srcRow, int count,
                                  const QModelIndex& dstParentIndex, int dstRow)
{
    /*
    Here parameters are passed in the same order (and with the same names)
     than in moveRows().
     That is we compute the undo operation of a moveRows called with these parameters.
     Thus src & dst should be interpreted as source & destination for the original move operation.
    */

    if(!srcParentIndex.isValid() || !dstParentIndex.isValid())
    {
        return false;
    }

    if(srcParentIndex == dstParentIndex && (srcRow <= dstRow && dstRow <= srcRow + count - 1))
    {
        return false;
    }

    Node* srcParent = nodeFromModelIndex(srcParentIndex);
    Q_ASSERT(srcParent);

    /*
    if (srcRow+count > srcParent->childCount())
    return false;
    */

    if(srcParentIndex == dstParentIndex)
    {
        //move up or down inside the same parent

        Node* parent = srcParent;
        Q_ASSERT(parent);

        if(srcRow > dstRow)
        {

            beginMoveRows(dstParentIndex, dstRow, dstRow + count - 1, srcParentIndex, srcRow + count);

            for(int i = 0; i < count; ++i)
            {
                Node* n = parent->takeChild(dstRow);
                parent->insertChild(srcRow + count - 1, n);
            }
        }
        else
        {
            Q_ASSERT(srcRow < dstRow);

            beginMoveRows(dstParentIndex, dstRow - count, dstRow - count + count - 1, srcParentIndex, srcRow);

            for(int i = 0; i < count; ++i)
            {

                Node* n = parent->takeChild(dstRow - count + i);
                parent->insertChild(srcRow + i, n);
            }
        }

    }
    else
    {
        //different parents

        beginMoveRows(dstParentIndex, dstRow, dstRow + count - 1, srcParentIndex, srcRow);

        Node* dstParent = nodeFromModelIndex(dstParentIndex);
        Q_ASSERT(dstParent);
        Q_ASSERT(dstParent != srcParent);

        for(int i = 0; i < count; ++i)
        {
            Node* n = dstParent->takeChild(dstRow);
            srcParent->insertChild(srcRow + i, n);
        }

    }

    endMoveRows();

    return true;
}


namespace
{
    const QString MimeTypeDevice = "application/DeviceExplorer.device.z";
    const QString MimeTypeAddress = "application/DeviceExplorer.address.z";

    const int compressionLevel = 6; //[0:no comrpession/fast ; 9:high compression/slow], -1: default zlib compression
}


/*
Drag and drop works by deleting the dragged items and creating a new set of dropped items that match those dragged.
I will/may call insertRows(), removeRows(), dropMimeData(), ...


We define two MimeTypes : MimeTypeDevice & MimeTypeAddress.
It allows to distinguish whether we are drag'n dropping devices or addresses.

 */


Qt::DropActions
DeviceExplorerModel::supportedDropActions() const
{
    return (Qt::CopyAction | Qt::MoveAction);
}

//Default supportedDragActions() implementation returns supportedDropActions().

Qt::DropActions
DeviceExplorerModel::supportedDragActions() const
{
    return (Qt::CopyAction | Qt::MoveAction);
}


QStringList
DeviceExplorerModel::mimeTypes() const
{
    return QStringList() << MimeTypeDevice << MimeTypeAddress; //TODO: add an xml MimeType to support drop of namespace XML file ?
}

//method called when a drag is initiated
QMimeData*
DeviceExplorerModel::mimeData(const QModelIndexList& indexes) const
{
    Q_ASSERT(indexes.count());

    //we drag only one node (and its children recursively).
    if(indexes.count() != 1)
    {
        return nullptr;
    }

    const QModelIndex index = indexes.at(0);

    Node* n = nodeFromModelIndex(index);

    if(n)
    {

        Q_ASSERT(n != m_rootNode);  //m_rootNode not displayed thus should not be draggable

        QByteArray dataC;
        /*
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        BinaryWriter bw(stream);
        bw.write(n);
        std::cerr<<"mimeData(): before compression: size="<<data.size()<<" capacity="<<data.capacity()<<"\n";
        dataC = qCompress(data, compressionLevel);
        std::cerr<<"mimeData(): after compression: size="<<dataC.size()<<" capacity="<<dataC.capacity()<<"\n";
        */
        #ifndef QT_NO_DEBUG
        const bool getOk =
        #endif
            getTreeData(index, dataC);
        Q_ASSERT(getOk);

        QMimeData* mimeData = new QMimeData;

        if(n->isDevice())
        {
            mimeData->setData(MimeTypeDevice, dataC);
        }
        else
        {
            mimeData->setData(MimeTypeAddress, dataC);
        }

        return mimeData;
    }

    return nullptr;
}


bool
DeviceExplorerModel::canDropMimeData(const QMimeData* mimeData,
                                     Qt::DropAction action,
                                     int /*row*/, int /*column*/, const QModelIndex& parent) const
{
    if(action == Qt::IgnoreAction)
    {
        return true;
    }

    if(action != Qt::MoveAction && action != Qt::CopyAction)
    {
        std::cerr << "dropMimeData(): not Qt::MoveAction or Qt::CopyAction ! NOT DONE \n";
        return false;
    }

    if(! mimeData || (! mimeData->hasFormat(MimeTypeDevice) && ! mimeData->hasFormat(MimeTypeAddress)))
    {
        return false;
    }


    Node* parentNode = nodeFromModelIndex(parent);

    if(mimeData->hasFormat(MimeTypeAddress))
    {
        if(parentNode == m_rootNode)
        {
            return false;
        }
    }
    else
    {
        Q_ASSERT(mimeData->hasFormat(MimeTypeDevice));

        if(parentNode != m_rootNode)
        {
            return false;
        }
    }

    return true;
}


//method called when a drop occurs
//return true if drop really handled, false otherwise.
//
// if dropMimeData returns true && action==Qt::MoveAction, removeRows is called immediately after
bool
DeviceExplorerModel::dropMimeData(const QMimeData* mimeData,
                                  Qt::DropAction action,
                                  int row, int column, const QModelIndex& parent)
{
    if(action == Qt::IgnoreAction)
    {
        return true;
    }

    if(action != Qt::MoveAction && action != Qt::CopyAction)
    {
        std::cerr << "dropMimeData(): not Qt::MoveAction or Qt::CopyAction ! NOT DONE \n";
        return false;
    }

    if(! mimeData || (! mimeData->hasFormat(MimeTypeDevice) && ! mimeData->hasFormat(MimeTypeAddress)))
    {
        return false;
    }

    /*
    if (column > 0) {
    std::cerr<<"dropMimeData(): tried to drop on column="<<column<<" ! NOT DONE\n";
    return false;
    }
    */

    // Unused:
//	if (column != NAME_COLUMN)
//		column = NAME_COLUMN;

    QModelIndex parentIndex; //invalid
    Node* parentNode = m_rootNode;
    QString mimeType = MimeTypeDevice;

    if(mimeData->hasFormat(MimeTypeAddress))
    {
        parentIndex = parent;
        parentNode = nodeFromModelIndex(parent);
        mimeType = MimeTypeAddress;

        if(parentNode == m_rootNode)
        {
            std::cerr << "dropMimeData(): cannot drop an address at device level \n";
            return false;
        }

    }
    else
    {
        Q_ASSERT(mimeData->hasFormat(MimeTypeDevice));
        Q_ASSERT(mimeType == MimeTypeDevice);
    }

    if(parentNode)
    {

        if(row == -1)
        {
            row = parentNode->childCount();    //parent.isValid() ? parent.row() : parentNode->childCount();
        }
        DeviceExplorer::Command::Insert* cmd = new DeviceExplorer::Command::Insert;
        const QString actionStr = (action == Qt::MoveAction ? tr("move") : tr("copy"));
        cmd->set(Path(parentNode), row, mimeData->data(mimeType), tr("Drop (%1)").arg(actionStr), iscore::IDocument::path(this));
        Q_ASSERT(m_cmdQ);
        m_cmdQ->redoAndPush(cmd);

        if(! m_cmdCreator->m_cachedResult)
        {
            delete m_cmdQ->command(0);
            m_cmdCreator->m_cachedResult = true;
            return false;
        }

        return true;

    }

    return false;
}


bool
DeviceExplorerModel::getTreeData(const QModelIndex& index, QByteArray& dataC) const
{
    if(! index.isValid())
    {
        return false;
    }

    Node* n = nodeFromModelIndex(index);

    if(n)
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        BinaryWriter bw(stream);
        bw.write(n);
        //std::cerr<<" data before compression: size="<<data.size()<<" capacity="<<data.capacity()<<"\n";
        dataC = qCompress(data, compressionLevel);
        //std::cerr<<" data after compression: size="<<dataC.size()<<" capacity="<<dataC.capacity()<<"\n";

        return true;
    }

    return false;
}

bool
DeviceExplorerModel::insertTreeData(const QModelIndex& parent, int row, const QByteArray& dataC)
{
    Node* parentNode = nodeFromModelIndex(parent);

    if(parentNode)
    {
        QByteArray data = qUncompress(dataC);
        QDataStream stream(&data, QIODevice::ReadOnly);
        BinaryReader br(stream);

        beginInsertRows(parent, row, row);

        br.read(parentNode);

        endInsertRows();

        return true;
    }

    return false;
}

void
DeviceExplorerModel::setCachedResult(Result r)
{
    m_cmdCreator->setCachedResult(r);
}

QModelIndex
DeviceExplorerModel::pathToIndex(const Path& path)
{
    QModelIndex iter;
    const int pathSize = path.size();

    for(int i = 0; i < pathSize; ++i)
    {
        iter = index(path.at(i), 0, iter);
    }

    return iter;
}

DeviceExplorerCommandCreator *DeviceExplorerModel::cmdCreator()
{
    return m_cmdCreator;
}

void
DeviceExplorerModel::serializePath(QDataStream& d, const Path& p)
{
    const int size = p.size();
    d << (qint32) size;

    for(int i = 0; i < size; ++i)
    {
        d << (qint32) p.at(i);
    }
}

void
DeviceExplorerModel::deserializePath(QDataStream& d, Path& p)
{
    qint32 size;
    d >> size;
    p.reserve(size);

    for(int i = 0; i < size; ++i)
    {
        qint32 v;
        d >> v;
        p.append(v);
    }
}


void
DeviceExplorerModel::debug_printPath(const Path& path)
{
    const int pathSize = path.size();

    for(int i = 0; i < pathSize; ++i)
    {
        std::cerr << path.at(i) << " ";
    }

    std::cerr << "\n";
}

void
DeviceExplorerModel::debug_printIndexes(const QModelIndexList& indexes)
{
    std::cerr << "indexes: " << indexes.size() << " nodes: \n";
    foreach(const QModelIndex & index, indexes)
    {
        if(index.isValid())
        {
            std::cerr << " index.row=" << index.row() << " col=" << index.column() << " ";
            Node* n = nodeFromModelIndex(index);

            if(n)
            {
                std::cerr << " n=" << n << " ";
                Node* parent = n->parent();

                if(n == m_rootNode)
                {
                    std::cerr << " rootNode parent=" << parent << "\n";
                }
                else
                {
                    std::cerr << " n->name=" << n->name().toStdString();
                    std::cerr << " parent=" << parent;
                    std::cerr << " parent->name=" << parent->name().toStdString() << "\n";
                }
            }
            else
            {
                std::cerr << " invalid node\n";
            }
        }
        else
        {
            std::cerr << " invalid index \n";
        }
    }
}
