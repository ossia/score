#include "DeviceExplorerModel.hpp"

#include <cassert>
#include <iostream>


#include <QFont>
#include <QBrush>
#include <QMimeData>

#include <QDebug>


#include <core/presenter/command/CommandQueue.hpp>
#include "Commands/DeviceExplorerMoveCommand.hpp"
#include "Commands/DeviceExplorerInsertCommand.hpp"
#include "Commands/DeviceExplorerCutCommand.hpp"
#include "Commands/DeviceExplorerPasteCommand.hpp"

enum {
  NAME_COLUMN=0,
  VALUE_COLUMN,
  //START_ASSIGNATION_COLUMN,
  //START_COLUMN,
  //INTERPOLATION_COLUMN,
  //END_ASSIGNATION_COLUMN,
  //END_COLUMN,
  //REDUNDANCY_COLUMN,
  //SR_COLUMN,
  IOTYPE_COLUMN,
  MIN_COLUMN,
  MAX_COLUMN,
  PRIORITY_COLUMN,
  
  COLUMN_COUNT //always last
};


static QString HEADERS[COLUMN_COUNT];

class Node
{
public:
  static QString INVALID_STR;
  static float INVALID_FLOAT;

  typedef enum {Invalid, In, Out, InOut} IOType;

  Node(const QString &name = QString(), Node *parent = nullptr)
    : m_name(name), 
      //m_startAssignation(false),
      //m_interpolation(false),
      //m_endAssignation(false),
      //m_redundancy(false),
      //m_sr(INVALID_FLOAT),
      m_ioType(Invalid),
      m_min(INVALID_FLOAT),
      m_max(INVALID_FLOAT),
      m_parent(parent)
  {
    if (m_parent)
      m_parent->addChild(this);
  }

  ~Node()
  {
    qDeleteAll(m_children); //calls delete on each children
  }

  Node *parent() const { return m_parent; }
  Node *childAt(int index) const { return m_children.value(index); } //return 0 if invalid index
  int indexOfChild(Node *child) const { return m_children.indexOf(child); } //return -1 if not found
  int childCount() const { return m_children.count(); }
  bool hasChildren() const { return ! m_children.empty(); }
  QList<Node *> children() const { return m_children; }
  
  void insertChild(int index, Node *n)
  {
    assert(n);
    n->m_parent = this;
    m_children.insert(index, n);

    //TODO: change corresponding API::Address ?
  }

  void addChild(Node *n)
  {
    assert(n);
    n->m_parent = this;
    m_children.append(n);

    //TODO: change corresponding API::Address ?
  }    
  
  void swapChildren(int oldIndex, int newIndex)
  {
    Q_ASSERT(oldIndex < m_children.count());
    Q_ASSERT(newIndex < m_children.count());

    m_children.swap(oldIndex, newIndex);
  }

  Node *takeChild(int index)
  {
    Node *n = m_children.takeAt(index);
    assert(n);
    n->m_parent = 0;
    return n;
  }

  //- accessors

  QString name() const { return m_name; }
  QString value() const { return m_value; }
  //bool startAssignation() const { return m_startAssignation; }
  //QString start() const { return m_start; }
  //bool interpolation() const { return m_interpolation; }
  //bool endAssignation() const { return m_endAssignation; }
  //QString end() const { return m_end; }
  //bool redundancy() const { return m_redundancy; }
  //float sr() const { return m_sr; }
  Node::IOType ioType() const { return m_ioType; }
  float minValue() const { return m_min; }
  float maxValue() const { return m_max; }
  unsigned int priority() const { return m_priority; }

  void setName(const QString &name) { m_name = name; }
  void setValue(const QString &value) { m_value = value; }
  //void setStartAssignation(bool startAssignation) { m_startAssignation = startAssignation; }
  //void setStart(const QString &start) { m_start = start; }
  //void setInterpolation(bool interpolation) { m_interpolation = interpolation; }
  //void setEndAssignation(bool endAssignation) { m_endAssignation = endAssignation; }
  //void setEnd(const QString &end) { m_end = end; }
  //void setRedundancy(bool redundancy) { m_redundancy = redundancy; }
  //void setSR(float sr) { m_sr = sr; }
  void setIOType(const Node::IOType ioType) { m_ioType = ioType; }
  void setMinValue(float minV) { m_min = minV; }
  void setMaxValue(float maxV) { m_max = maxV; }
  void setPriority(unsigned int priority) { m_priority = priority; }

  /*
  void setStartAssignationRecursive(bool startAssignation) 
  { 
    setStartAssignation(startAssignation); 
    for (int i=0; i<m_children.count(); ++i)
      m_children.at(i)->setStartAssignationRecursive(startAssignation);
  }

  void setEndAssignationRecursive(bool endAssignation) 
  { 
    setEndAssignation(endAssignation); 
    for (int i=0; i<m_children.count(); ++i)
      m_children.at(i)->setEndAssignationRecursive(endAssignation);
  }
  */


  bool isSelectable() const 
  { 
    return true; 
    //return m_ioType != Node::In;
  } //TODO: or has a child of ioType != Node::In !!!

  bool isEditable() const { return m_ioType == Node::Out || m_ioType == Node::Invalid; }
  
  bool isDevice() const 
  { 
    if (parent()) { 
      return parent()->parent() == nullptr; 
    }
    return false;
  }

  /*
  bool startAssignationChecked() const { return m_startAssignation; }
  bool endAssignationChecked() const { return m_endAssignation; }

  void startAssignationNbChecks(int &nbChecks, int &nbLeafs) const
  {
    nbChecks = 0;
    nbLeafs = 0;
    startAssignationNbChecks_aux(nbChecks, nbLeafs);
  }

  void endAssignationNbChecks(int &nbChecks, int &nbLeafs) const
  {
    nbChecks = 0;
    nbLeafs = 0;
    endAssignationNbChecks_aux(nbChecks, nbLeafs);
  }
  */
  
  Node *clone() const
  {
    
    Node *n = new Node(*this);
    const int numChildren = n->childCount();
    n->m_children.clear();
    n->m_children.reserve(numChildren);
    for (int i=0; i<numChildren; ++i) {
      n->m_children.append( m_children.at(i)->clone() );
    }
    return n;
  }



protected:

  /*
  void startAssignationNbChecks_aux(int &nbChecks, int &nbLeafs) const
  {
    if (m_children.empty()) {
      nbChecks += startAssignationChecked();
      ++nbLeafs;
    }
    else {
      for (int i=0; i<m_children.size(); ++i) {
	m_children.at(i)->startAssignationNbChecks_aux(nbChecks, nbLeafs);
      }
    }
    assert(nbChecks <= nbLeafs);
  }

  void endAssignationNbChecks_aux(int &nbChecks, int &nbLeafs) const
  {
    if (m_children.empty()) {
      nbChecks += endAssignationChecked();
      ++nbLeafs;
    }
    else {
      for (int i=0; i<m_children.size(); ++i) {
	m_children.at(i)->endAssignationNbChecks_aux(nbChecks, nbLeafs);
      }
    }
    assert(nbChecks <= nbLeafs);
  }
  */



protected: 

  QString m_name;
  QString m_value;
  //bool m_startAssignation;
  //QString m_start;
  //bool m_interpolation;
  //bool m_endAssignation;
  //QString m_end;
  //bool m_redundancy;
  //float m_sr;
  IOType m_ioType;
  float m_min;
  float m_max;
  unsigned int m_priority;
  Node *m_parent;
  QList<Node *> m_children;
};

QString Node::INVALID_STR = "-_-";
float Node::INVALID_FLOAT = std::numeric_limits<float>::max();


class BinaryWriter
{
  QDataStream &m_stream;

public:
  BinaryWriter(QDataStream &stream)
    : m_stream(stream)
  {

  }

  void write(const Node *n)
  {
    Q_ASSERT(n);

    m_stream << n->name();
    //TODO: type !
    m_stream << n->value();
    m_stream << (qint32)n->ioType();
    m_stream << n->minValue();
    m_stream << n->maxValue();
    m_stream << (quint32)n->priority();
    //TODO: cropMode, tags, ...

    m_stream << (quint32)n->childCount();
    foreach (Node *child, n->children())
      write(child);
    
  }

};

class BinaryReader
{
  QDataStream &m_stream;

public:
  BinaryReader(QDataStream &stream)
    : m_stream(stream)
  {
  }

  void read(Node *parent)
  {
    Q_ASSERT(parent);
    
    QString name;
    m_stream >> name;

    Node *n = new Node(name, parent);

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
    for (quint32 i=0; i<childCount; ++i) {
      read(n);
    }

  }
};



DeviceExplorerModel::DeviceExplorerModel(QObject *parent)
  : QAbstractItemModel(parent),
    m_rootNode(nullptr),
    m_lastCutNodeIsCopied(false),
    m_cmdQ(nullptr),
    m_cachedResult(true)
{

  HEADERS[NAME_COLUMN] = tr("Address");
  HEADERS[VALUE_COLUMN] = tr("Value");
  //HEADERS[START_ASSIGNATION_COLUMN] = "v";
  //HEADERS[START_COLUMN] = tr("Start");
  //HEADERS[INTERPOLATION_COLUMN] = "~";
  //HEADERS[END_ASSIGNATION_COLUMN] = "v";
  //HEADERS[END_COLUMN] = tr("End");
  //HEADERS[REDUNDANCY_COLUMN] = "=";
  //HEADERS[SR_COLUMN] = "%";
  HEADERS[IOTYPE_COLUMN] = tr("I/O");
  HEADERS[MIN_COLUMN] = tr("min");
  HEADERS[MAX_COLUMN] = tr("max");
  HEADERS[PRIORITY_COLUMN] = tr("priority");

}

DeviceExplorerModel::~DeviceExplorerModel()
{
  delete m_rootNode;

  for (QStack<CutElt>::iterator it = m_cutNodes.begin(); 
       it != m_cutNodes.end(); ++it) {
    delete it->first;
  }

}

//TODO:REMOVE

#include <QTextStream>
#include <QFile>

//TODO:REMOVE
static
Node *
readNode(QTextStream &in, Node *parent)
{
  //const int nbValues = 13;
  QString name;
  in >> name;

  if (name.isEmpty())
     return NULL;
  
  if (in.status() != QTextStream::Ok) {
    std::cerr<<"status not ok (2)\n";
    exit(10);
  }

  if (in.atEnd())
    return 0;

  static const QString INVALID = "-_-";

  if (name == INVALID)
    return 0;

  Node *node = new Node(name, parent);
  
  QString value;
  in >> value;
  if (value != INVALID)
    node->setValue(value);

  QString startAssignation;
  in >> startAssignation;
  //if (startAssignation != INVALID)
  //node->setStartAssignation(startAssignation.toInt());
    
  QString start;
  in >> start;
  //if (start != INVALID)
  //node->setStart(start);
    
  QString interpolation;
  in >> interpolation;
  //if (interpolation != INVALID)
  //node->setInterpolation(interpolation.toInt());
  
  QString endAssignation;
  in >> endAssignation;
  //if (endAssignation != INVALID)
  //node->setEndAssignation(endAssignation.toInt());

  QString end;
  in >> end;
  //if (end != INVALID)
  //node->setEnd(end);
  
  QString redundancy;
  in >> redundancy;
  //if (redundancy != INVALID)
  //node->setRedundancy(redundancy.toInt());

  QString sr;
  in >> sr;
  //if (sr != INVALID)
  //node->setSR(sr.toFloat());

  QString iotype;
  in >> iotype;
  if (iotype != INVALID) {
    if (iotype == "->")
      node->setIOType(Node::In);
    else if (iotype == "<-")
      node->setIOType(Node::Out);
    else if (iotype == "<->")
      node->setIOType(Node::InOut);
  }

  QString minB;
  in >> minB;
  if (minB != INVALID)
    node->setMinValue(minB.toFloat());

  QString maxB;
  in >> maxB;
  if (maxB != INVALID)
    node->setMaxValue(maxB.toFloat());

  QString priority;
  in >> priority;
  if (priority != INVALID)
    node->setPriority(priority.toInt());

  int nbChildren = 0;
  in >> nbChildren;

  //node->_children.reserve(nbChildren);
  for (int i=0; i<nbChildren; ++i) {
    readNode(in, node); //add read child to node.
    //Node *n = readNode(in, node);
    //if (n != nullptr)
    // node->addChild(n);
  }

  return node;
}

bool
DeviceExplorerModel::load(const QString &filename)
{
  QFile file(filename);
  if (! file.open(QIODevice::ReadOnly))
    return false;

  QTextStream in(&file);
  
  Node *node = readNode(in, nullptr);
  if (node == nullptr) {
    std::cerr<<"Error: unabel to read: "<<filename.toStdString()<<"\n";
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
DeviceExplorerModel::setCommandQueue(iscore::CommandQueue *q)
{
  m_cmdQ = q;
}

QStringList
DeviceExplorerModel::getColumns() const
{
  QStringList l;
  for (int i=0; i<COLUMN_COUNT; ++i) {
    l.append(HEADERS[i]);
  }
  return l;
}

void
DeviceExplorerModel::addDevice(const QList<QString> &deviceSettings)
{
  Q_ASSERT(deviceSettings.size() >= 2);
  const QString protocol = deviceSettings.at(0);
  const QString deviceName = deviceSettings.at(1);
  
  if(m_rootNode == nullptr)
    m_rootNode = createRootNode();
  Q_ASSERT(m_rootNode);


  //we always insert as last child of m_rootNode
  //TODO: insert after current selected device ???

  int row = m_rootNode->childCount();
  QModelIndex parent; //invalid
  beginInsertRows(parent, row, row);

  //TODO: contact MainWindow or Model to get/build/explore hierarchy from these settings
  Node *node = new Node(deviceName, m_rootNode);
  {
    //DEBUG: arbitrary population of the tree

     if (protocol == "Minuit" || protocol == "OSC") {
       Node *node1 = new Node("debug1", node);
       node1->setValue("10"); node1->setIOType(Node::In); 
       node1->setMinValue(0.f); node1->setMaxValue(0.f);
       node1->setPriority(1);
       //node->addChild(node1);
       Node *node2 = new Node("debug2", node);
       node2->setValue("13.7"); node2->setIOType(Node::Out);
       node2->setMinValue(0.f); node2->setMaxValue(76.f);
       node2->setPriority(2);
       //node->addChild(node2);
     }
     if (protocol == "OSC" || protocol == "MIDI") {
        Node *node3 = new Node("debug3", node);
	node3->setValue("13"); node3->setIOType(Node::InOut);
	node3->setMinValue(0.f); node3->setMaxValue(100.f);
	node3->setPriority(2);
	//node->addChild(node3);
        Node *node4 = new Node("debug4", node3);
	node4->setValue("11"); node4->setIOType(Node::InOut);
	node4->setMinValue(1.f); node4->setMaxValue(78.f);
	node4->setPriority(7);
	//node3->addChild(node4);

	if (protocol == "OSC") {
	  Node *node5 = new Node("debug5", node4);
	  node5->setValue("777"); node5->setIOType(Node::In);
	  node5->setMinValue(1.f); node5->setMaxValue(3.f);
	  node5->setPriority(3);
	  
	  Node *node6 = new Node("debug6", node5);
	  node6->setValue("777"); node6->setIOType(Node::In);
	  node6->setMinValue(1.f); node6->setMaxValue(3.f);
	  node6->setPriority(3);
	  
	  Node *node7 = new Node("debug7", node5);
	  node7->setValue("754"); node7->setIOType(Node::Out);
	  node7->setMinValue(1.33f); node7->setMaxValue(2.3f);
	  node7->setPriority(33);
	}
     
     }
  }

  endInsertRows();

}

namespace {
  void setIOType(Node *n, const QString &type)
  {
    Q_ASSERT(n);
    if (type == "In")
      n->setIOType(Node::In);
    else if (type == "Out")
      n->setIOType(Node::Out);
    else if (type == "In/Out")
      n->setIOType(Node::InOut);
    else {
      std::cerr<<"Unknown I/O type: "<<type.toStdString()<<"\n";
    }
  }


}
  
void
DeviceExplorerModel::addAddress(QModelIndex index, DeviceExplorerModel::Insert insert, const QList<QString> &addressSettings)
{
  if (! index.isValid())
    return;

  Node *n = nodeFromModelIndex(index);
  Q_ASSERT(n);
  Q_ASSERT(n != m_rootNode);

  Node *parent = nullptr;
  int row = 0;
  if (insert == AsSibling) {
    parent = n->parent();
    
    if (parent == m_rootNode)
      return; //we cannot add an address at the device level.

    Q_ASSERT(parent);
    row = parent->indexOfChild(n)+1; //insert after index.
  }
  else if (insert == AsChild) {
    parent = n;
    row = n->childCount(); //insert as last child
  }
  Q_ASSERT(parent);
  Q_ASSERT(parent != m_rootNode);
  Node *grandparent = parent->parent();
  Q_ASSERT(grandparent);
  int rowParent = grandparent->indexOfChild(parent);
  QModelIndex parentIndex = createIndex(rowParent, 0, parent);
  
  beginInsertRows(parentIndex, row, row);
  
  /*
  std::cerr<<"DeviceExplorerModel::addAddress parent="<<parent<<"\n";
  std::cerr<<"DeviceExplorerModel::addAddress parent->name="<<parent->name().toStdString()<<"\n";
  std::cerr<<"addressSettings.size()="<<addressSettings.size()<<"\n";
  std::cerr<<"name="<<addressSettings.at(0).toStdString()<<"\n";
  std::cerr<<"row="<<row<<"\n";
  */

  Q_ASSERT(addressSettings.size() >= 2);
  QString name = addressSettings.at(0);
  QString valueType = addressSettings.at(1);

  Node *node = new Node(name, nullptr);//build without parent otherwise appended at the end
  if (valueType == "Int") {
    QString ioType = addressSettings.at(2);
    QString value = addressSettings.at(3);
    QString valueMin = addressSettings.at(4);
    QString valueMax = addressSettings.at(5);
    QString unite = addressSettings.at(6);
    QString clipMode = addressSettings.at(7);
    QString priority = addressSettings.at(8);
    QString tags = addressSettings.at(9);
    node->setValue(value);
    setIOType(node, ioType);
    node->setMinValue(valueMin.toUInt());
    node->setMaxValue(valueMax.toUInt());
    node->setPriority(priority.toUInt());
    //TODO: other columns
  }
  else if (valueType == "Float") {
    QString ioType = addressSettings.at(2);
    QString value = addressSettings.at(3);
    QString valueMin = addressSettings.at(4);
    QString valueMax = addressSettings.at(5);
    QString unite = addressSettings.at(6);
    QString clipMode = addressSettings.at(7);
    QString priority = addressSettings.at(8);
    QString tags = addressSettings.at(9);
    node->setValue(value);
    setIOType(node, ioType);
    node->setMinValue(valueMin.toFloat());
    node->setMaxValue(valueMax.toFloat());
    node->setPriority(priority.toUInt());
    //TODO: other columns
  }
  else if (valueType == "String") {

    QString ioType = addressSettings.at(2);
    QString value = addressSettings.at(3);
    QString priority = addressSettings.at(4);
    QString tags = addressSettings.at(5);
    node->setValue(value);
    node->setPriority(priority.toUInt());
    setIOType(node, ioType);
  }
  parent->insertChild(row, node);

  endInsertRows();
}


QModelIndex
DeviceExplorerModel::index(int row, int column, const QModelIndex &parent) const
{
  if (!m_rootNode || row < 0 || column < 0 || column >= COLUMN_COUNT)
    return QModelIndex();

  Node *parentNode = nodeFromModelIndex(parent);
  assert(parentNode);
  Node *childNode = parentNode->childAt(row); //value() return 0 if out of bounds
  if (! childNode) 
    return QModelIndex();

  return createIndex(row, column, childNode);
}

Node *
DeviceExplorerModel::nodeFromModelIndex(const QModelIndex &index) const
{
  if (index.isValid())
    return static_cast<Node *>(index.internalPointer());
  else
    return m_rootNode;
}

QModelIndex
DeviceExplorerModel::parent(const QModelIndex &child) const
{
  Node *node = nodeFromModelIndex(child);
  if (! node)
    return QModelIndex();

  Node *parentNode = node->parent();
  if (! parentNode)
    return QModelIndex();

  Node *grandparentNode = parentNode->parent();
  if (! grandparentNode)
    return QModelIndex();

  const int rowParent = grandparentNode->indexOfChild(parentNode); //(return -1 if not found)
  assert(rowParent != -1);
  return createIndex(rowParent, 0, parentNode);
}

int
DeviceExplorerModel::rowCount(const QModelIndex &parent) const
{
  if (parent.column() > 0)
    return 0;

  Node *parentNode = nodeFromModelIndex(parent);
  if (! parentNode)
    return 0;
  
  return parentNode->childCount();
}


int
DeviceExplorerModel::columnCount() const
{
  return COLUMN_COUNT;
}

int
DeviceExplorerModel::columnCount(const QModelIndex &/*parent*/) const
{
  return COLUMN_COUNT;
}

// must return an invalid QVariant for cases not handled
QVariant
DeviceExplorerModel::data(const QModelIndex &index, int role) const
{
  //if (role != Qt::DisplayRole) 
  //return QVariant();

  const int col = index.column();
  if (col < 0 || col >= COLUMN_COUNT)
    return QVariant();

  Node *node = nodeFromModelIndex(index);
  if (! node)
    return QVariant();

  switch (col) {
  case NAME_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	return node->name();
      }
      else if (role == Qt::FontRole) {
	const Node::IOType ioType = node->ioType();
	if (ioType == Node::In || ioType == Node::Out) {
	  QFont f; // = QAbstractItemModel::data(index, role); //B: how to get current font ?
	  f.setItalic(true);
	  return f;
	}
      }
      else if (role == Qt::ForegroundRole) {
	const Node::IOType ioType = node->ioType();
	if (ioType == Node::In || ioType == Node::Out) {
	  return QBrush(Qt::black);
	}
      }

    }
    break;
  case VALUE_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	return node->value();
      }
      else if (role == Qt::ForegroundRole) {
	const Node::IOType ioType = node->ioType();
	if (ioType == Node::In || ioType == Node::Out) {
	  return QBrush(Qt::black);
	}
      }
    }
    break;
    /*
  case START_ASSIGNATION_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	return QVariant();
	//return QVariant((bool)(node->startAssignation()));
      }
      else if (role == Qt::CheckStateRole) {
	int nbChecks, nbLeafs;
	node->startAssignationNbChecks(nbChecks, nbLeafs);
	if (nbChecks > 0) //TODO: use ratio nbChecks/nbLeafs to draw icon 
	  return Qt::Checked;
	else
	  return Qt::Unchecked;
      }
    }
    break;
  case START_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	return node->start();
      }
    }
    break;
  case INTERPOLATION_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	return QVariant();
	//return node->interpolation();
      }
      else if (role == Qt::CheckStateRole) {
	if (node->hasChildren())
	  return QVariant();
	const bool checked = node->interpolation();
	if (checked) 
	  return Qt::Checked;
	else
	  return Qt::Unchecked;
      }
    }
    break;
  case END_ASSIGNATION_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	return QVariant();
	//return QVariant((bool)(node->endAssignation()));
      }
      else if (role == Qt::CheckStateRole) {
	int nbChecks, nbLeafs;
	node->endAssignationNbChecks(nbChecks, nbLeafs);
	if (nbChecks > 0) //TODO: use ratio nbChecks/nbLeafs to draw icon 
	  return Qt::Checked;
	else
	  return Qt::Unchecked;
      }
    }
    break;
  case END_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	return node->end();
      }
    }
    break;
  case REDUNDANCY_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	return QVariant();
	//return QVariant((bool)(node->redundancy()));
      }
      else if (role == Qt::CheckStateRole) {
	if (node->hasChildren())
	  return QVariant();
	const bool checked = node->redundancy();
	if (checked) 
	  return Qt::Checked;
	else
	  return Qt::Unchecked;
      }
    }
    break;
  case SR_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	float sr = node->sr();
	if (sr != Node::INVALID_FLOAT)
	  return sr;
	return QString();
      }
     }
    break;
    */
  case IOTYPE_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	switch (node->ioType()) {
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
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	const float minV = node->minValue();
	if (minV != Node::INVALID_FLOAT)
	  return minV;
	return QString();
      }
    }
    break;
  case MAX_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	const float maxV = node->maxValue();
	if (maxV != Node::INVALID_FLOAT)
	  return maxV;
	return QString();
      }
    }
    break;
  case PRIORITY_COLUMN:
    {
      if (role == Qt::DisplayRole || role == Qt::EditRole) {
	return node->priority();
      }
    }
    break;
  default :
    assert(false);
    return QString();
  }
  
  //also
  //role == Qt::TextAlignmentRole
  //role == Qt::DecorationRole



  return QVariant();
}

/*
QVariant
DeviceExplorerModel::getColumnValue(Node *node, int col) const
{
  if (col < 0 || col >= COLUMN_COUNT)
    return QString();

  assert(node);

  switch (col) {
  case NAME_COLUMN:
    {
      return node->name();
    }
    break;
  case VALUE_COLUMN:
    {
      return node->value();
    }
    break;
  case START_ASSIGNATION_COLUMN:
    {
      return QVariant();
      //return QVariant((bool)(node->startAssignation()));
    }
    break;
  case START_COLUMN:
    {
      return node->start();
    }
    break;
  case INTERPOLATION_COLUMN:
    {
      return QVariant();
      //return node->interpolation();
    }
    break;
  case END_ASSIGNATION_COLUMN:
    {
      return QVariant();
      //return QVariant((bool)(node->endAssignation()));
    }
    break;
  case END_COLUMN:
    {
      return node->end();
    }
    break;
  case REDUNDANCY_COLUMN:
    {
      return QVariant();
      //return QVariant((bool)(node->redundancy()));
    }
    break;
  case SR_COLUMN:
    {
      float sr = node->sr();
      if (sr != Node::INVALID_FLOAT)
        return sr;
      return QString();
     }
    break;
  case IOTYPE_COLUMN:
    {
      return node->type();
    }
    break;
  case MIN_COLUMN:
    {
      const float minV = node->minValue();
      if (minV != Node::INVALID_FLOAT)
        return minV;
      return QString();
    }
    break;
  case MAX_COLUMN:
    {
      const float maxV = node->maxValue();
      if (maxV != Node::INVALID_FLOAT)
	return maxV;
      return QString();
    }
    break;
  case PRIORITY_COLUMN:
    {
      return node->priority();
    }
    break;
  default :
    assert(false);
    return QString();
  }

  return QString();
}
*/

void
DeviceExplorerModel::setColumnValue(Node *node, const QVariant &v, int col) 
{
  if (col < 0 || col >= COLUMN_COUNT)
    return;

  assert(node);

  switch (col) {
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
    /*
  case START_ASSIGNATION_COLUMN:
    {
      node->setStartAssignation(v.toBool());
    }
    break;
  case START_COLUMN:
    {
      node->setStart(v.toString());
    }
    break;
  case INTERPOLATION_COLUMN:
    {
      node->setInterpolation(v.toBool());
    }
    break;
  case END_ASSIGNATION_COLUMN:
    {
      node->setEndAssignation(v.toBool());
    }
    break;
  case END_COLUMN:
    {
      node->setEnd(v.toString());
    }
    break;
  case REDUNDANCY_COLUMN:
    {
      node->setRedundancy(v.toBool());
    }
    break;
  case SR_COLUMN:
    {
      node->setSR(v.toFloat());
     }
    break;
    */
  case IOTYPE_COLUMN:
    {
      //node->setType(v.toString());
    }
    break;
  case MIN_COLUMN:
    {
      node->setMinValue(v.toFloat()); //TODO:change ! no necessarily a float !
    }
    break;
  case MAX_COLUMN:
    {
      node->setMaxValue(v.toFloat()); //TODO:change ! no necessarily a float !
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
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    if (section >=0 && section <COLUMN_COUNT)
      return HEADERS[section];
  }
  return QVariant();
}

Qt::ItemFlags
DeviceExplorerModel::flags(const QModelIndex &index) const
{
  Qt::ItemFlags f = Qt::ItemIsEnabled;
  //by default QAbstractItemModel::flags(index); returns Qt::ItemIsEnabled | Qt::ItemIsSelectable

  if (index.isValid()) {
    Node *n = nodeFromModelIndex(index);
    assert(n);

    if (n->isSelectable()) {
      f |= Qt::ItemIsSelectable;
    }
    
    /*
    const int col = index.column();
    if (col == START_ASSIGNATION_COLUMN 
	|| col == END_ASSIGNATION_COLUMN) {
      f |= Qt::ItemIsUserCheckable;
    }
    else if (col == INTERPOLATION_COLUMN 
	     || col == REDUNDANCY_COLUMN) {
      if (! n->hasChildren()) {
	f |= Qt::ItemIsUserCheckable;
      }
    }
    */

    //we allow drag'n drop only from the name column
    if (index.column() == NAME_COLUMN) {
      f |= Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled;
    }


    if (n->isEditable()) {
      //std::cerr<<"DeviceExplorerModel::flags "<<n->name().toStdString()<<" ioType="<<(int)n->ioType()<<" Qt::ItemIsEditable\n";

      f |= Qt::ItemIsEditable;
    }

  }
  else {
    //to be able to drop even where there is nothing
    f |= Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled;

  }

  return f;
}

/*
  return false if no change was made.
  emit dataChanged() & return true if a change is made.
*/
bool
DeviceExplorerModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (! index.isValid())
    return false;

  Node *node = nodeFromModelIndex(index);
  if (! node)
    return false;

  bool changed = false;
  QModelIndex changedTopLeft = index;
  QModelIndex changedBottomRight = index;


  //TODO: check what's editable or not !!!

  if (role == Qt::EditRole) {
    if (index.column() == NAME_COLUMN) {
      const QString s = value.toString();
      if (! s.isEmpty()) {
	node->setName(s);
	changed = true;
      }
    }
    
    if (index.column() == IOTYPE_COLUMN) {
      const QString iotype = value.toString();
      if (iotype == "->")
	node->setIOType(Node::In);
      else if (iotype == "<-")
	node->setIOType(Node::Out);
      else if (iotype == "<->")
	node->setIOType(Node::InOut);
      changed = true;
    }
    

  }
  /*
  else if (role == Qt::CheckStateRole) {
    const bool checked = value.toBool();
    if (index.column() == START_ASSIGNATION_COLUMN) {
      node->setStartAssignationRecursive(checked);
      changedBottomRight = bottomIndex(index);
      changed = true;
    }
    else if (index.column() == END_ASSIGNATION_COLUMN) {
      node->setEndAssignationRecursive(checked);
      changedBottomRight = bottomIndex(index);
      changed = true;
    }
    else if (index.column() == INTERPOLATION_COLUMN) {
      node->setInterpolation(checked);
      changed = true;
    }
    else if (index.column() == REDUNDANCY_COLUMN) {
      node->setRedundancy(checked);
      changed = true;
    }
  }
  */

  //NON ! emitDatachanged devrait être fait pour tous les indices des fils !!!
  
  if (changed) {

    {
      Node *n1 = nodeFromModelIndex(changedTopLeft);
      Node *n2 = nodeFromModelIndex(changedBottomRight);
      std::cerr<<"DeviceExplorerModel::setData i1=("<<n1->name().toStdString()<<", "<<changedTopLeft.row()<<", "<<changedTopLeft.column()<<") i2="<<n2->name().toStdString()<<", "<<changedBottomRight.row()<<", "<<changedBottomRight.column()<<")\n";
    }

    emit dataChanged(changedTopLeft, changedBottomRight);
    return changed; //true;
  }


  return changed; //false;
}

bool
DeviceExplorerModel::setHeaderData(int, Qt::Orientation, const QVariant&, int)
{
  return false; //we prevent editing the (column) headers
}


QModelIndex
DeviceExplorerModel::bottomIndex(const QModelIndex &index) const
{
  Node *node = nodeFromModelIndex(index);

  if (! node)
    return index;

  if (! node->hasChildren())
    return index;

  return bottomIndex(createIndex(node->childCount()-1, index.column(), node->childAt(node->childCount()-1)));
}

Node *
DeviceExplorerModel::createRootNode() const
{
  return new Node("Invisible root", nullptr);
}

/*
//this method is called (behind the scenes) when there is a drag and drop to insert drop rows
bool
DeviceExplorerModel::insertRows(int row, int count, const QModelIndex &parent)
{
  if (! m_rootNode)
    m_rootNode = createRootNode();

  std::cerr<<"***##### DeviceExplorerModel::insertRows("<<row<<", "<<count<<" ) ###### \n";

  Node *parentNode = parent.isValid() ? nodeFromModelIndex(parent) : m_rootNode;

  //TODO: if (parentNode == m_rootNode) : we insert count devices ! Is it what we want ???

  beginInsertRows(parent, row, row+count-1);

  for (int i=0; i<count; ++i) {
    (void) new Node(tr("default name"), parentNode);
  }
  
  endInsertRows();

  return true;
}
*/


//this method is called (behind the scenes) when there is a drag and drop to delete the original dragged rows once they have been dropped (dropped rows are inserted using insertRows)
bool
DeviceExplorerModel::removeRows(int row, int count, const QModelIndex &parent)
{
  if (! m_rootNode)
    return false;

  Node *parentNode = parent.isValid() ? nodeFromModelIndex(parent) : m_rootNode;

  beginRemoveRows(parent, row, row+count-1);
  for (int i=0; i<count; ++i) {

    Node *n = parentNode->takeChild(row);

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
  if (!index.isValid())
    return true;
  
  Node *n = nodeFromModelIndex(index);
  Q_ASSERT(n);
  
  Node *parent = n->parent();
  Q_ASSERT(parent);
  if (parent == m_rootNode)
    return true;
  else
    return false;
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

QModelIndex
DeviceExplorerModel::copy(const QModelIndex &index)
{
  if (!index.isValid())
    return Result(false, index);
  
  Node *n = nodeFromModelIndex(index);
  Q_ASSERT(n);

  Node *copiedNode = n->clone();
  const bool isDevice = n->isDevice();

  if (! m_cutNodes.isEmpty() && m_lastCutNodeIsCopied) {
    //necessary to avoid that several successive copies fill m_cutNodes (copy is not a command)
    Node *prevCopiedNode = m_cutNodes.pop().first;
    delete prevCopiedNode;
  }
  m_cutNodes.push(CutElt(copiedNode, isDevice));
  m_lastCutNodeIsCopied = true;

  return index;
}


DeviceExplorerModel::Result
DeviceExplorerModel::cut_aux(const QModelIndex &index)
{
  if (!index.isValid())
    return Result(false, index);
  

  Node *cutNode = nodeFromModelIndex(index);
  Q_ASSERT(cutNode);
  
  const bool cutNodeIsDevice = cutNode->isDevice();


  if (! m_cutNodes.isEmpty() && m_lastCutNodeIsCopied) {
    //necessary to avoid that several successive copies fill m_cutNodes (copy is not a command)
    Node *prevCopiedNode = m_cutNodes.pop().first;
    delete prevCopiedNode;
  }
  m_cutNodes.push(CutElt(cutNode, cutNodeIsDevice));
  m_lastCutNodeIsCopied = false;

  Node *parent = cutNode->parent();
  Q_ASSERT(parent);
  
  int row = parent->indexOfChild(cutNode);
  Q_ASSERT(row == index.row());
  
  beginRemoveRows(index.parent(), row, row);

#ifndef QT_NO_DEBUG
  Node *child = 
#endif
    parent->takeChild(row);
  Q_ASSERT(child == cutNode);

  endRemoveRows();
  
  //TODO: we should emit a signal to indicate that a paste is now possible !?!
  
  if (row > 0) {
    --row;
    return createIndex(row, 0, parent->childAt(row));
  }
  if (parent != m_rootNode) {
    Node *grandParent = parent->parent();
    Q_ASSERT(grandParent);
    return createIndex(grandParent->indexOfChild(parent), 0, parent);
  }
  return QModelIndex();
}

QModelIndex
DeviceExplorerModel::cut(const QModelIndex &index)
{
  if (!index.isValid())
    return index;

  DeviceExplorerCutCommand *cmd = new DeviceExplorerCutCommand;
  
  QString name = (index.isValid() ? nodeFromModelIndex(index)->name() : "");
  cmd->set(index.parent(), index.row(), tr("Cut %1").arg(name), this);
  Q_ASSERT(m_cmdQ);
  m_cmdQ->push(cmd);
  if (! m_cachedResult) {
    delete m_cmdQ->command(0);
    m_cachedResult = true;
    return m_cachedResult.index;
  }
  return m_cachedResult.index;
  
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
DeviceExplorerModel::paste_aux(const QModelIndex &index, bool after)
{
  if (m_cutNodes.isEmpty()) 
    return Result(false, index);

  if (! index.isValid() && ! m_cutNodes.top().second) //we can not pass addresses at top level
    return Result(false, index);


  //REM: we always paste as sibling
  

  Node *parent = nullptr;
  int row = 0;
  QModelIndex parentIndex;

  const bool cutNodeIsDevice = m_cutNodes.top().second;
  Node *cutNode = m_cutNodes.top().first;
  m_cutNodes.pop();

  if (index.isValid()) {
    Node *n = nodeFromModelIndex(index);
    Q_ASSERT(n);

    parent = n->parent();
    Q_ASSERT(parent);

    parentIndex = index.parent();

    if (cutNodeIsDevice) {
      //we can only paste devices at the top-level
      while (parent != m_rootNode) {
	Q_ASSERT(parent);
	n = parent;
	parent = parent->parent();
      }
      Q_ASSERT(parent->indexOfChild(n) != -1);

      parentIndex = QModelIndex(); //invalid on purpose

    }

    row = parent->indexOfChild(n) + (after ? 1 : 0);

  }
  else {
    Q_ASSERT(! index.isValid() && cutNodeIsDevice);
    parent = m_rootNode;
    row = m_rootNode->childCount();
    parentIndex = QModelIndex(); //invalid on purpose
  }
  Q_ASSERT(parent);

  Q_ASSERT(cutNode);

  beginInsertRows(parentIndex, row, row);
    
  parent->insertChild(row, cutNode);

  Node *child = cutNode;

  endInsertRows();

  return createIndex(row, 0, child);
}

DeviceExplorerModel::Result
DeviceExplorerModel::pasteAfter_aux(const QModelIndex &index)
{
  return paste_aux(index, true);
}

DeviceExplorerModel::Result
DeviceExplorerModel::pasteBefore_aux(const QModelIndex &index)
{
  return paste_aux(index, false);
}


QModelIndex
DeviceExplorerModel::paste(const QModelIndex &index)
{
  if (m_cutNodes.isEmpty()) 
    return index;

  if (! index.isValid() && ! m_cutNodes.top().second) //we can not pass addresses at top level
    return index;


  DeviceExplorerPasteCommand *cmd = new DeviceExplorerPasteCommand;
  
  QString name = (index.isValid() ? nodeFromModelIndex(index)->name() : "");
  cmd->set(index.parent(), index.row(), tr("Paste %1").arg(name), this);
  Q_ASSERT(m_cmdQ);
  m_cmdQ->push(cmd);
  if (! m_cachedResult) {
    delete m_cmdQ->command(0);
    m_cachedResult = true;
    return m_cachedResult.index;
  }
  return m_cachedResult.index;  
}

bool
DeviceExplorerModel::moveRows(const QModelIndex &srcParentIndex, int srcRow, int count, 
			   const QModelIndex &dstParentIndex, int dstRow)
{
  if (!srcParentIndex.isValid() || !dstParentIndex.isValid())
    return false;
  if (srcParentIndex == dstParentIndex && (srcRow <= dstRow && dstRow <= srcRow+count-1) )
    return false;
  
  Node *srcParent = nodeFromModelIndex(srcParentIndex);
  Q_ASSERT(srcParent);

  if (srcRow+count > srcParent->childCount())
    return false;


  beginMoveRows(srcParentIndex, srcRow, srcRow+count-1, dstParentIndex, dstRow);

  if (srcParentIndex == dstParentIndex) {
    //move up or down inside the same parent

    Node *parent = srcParent;
    Q_ASSERT(parent);
    
    if (srcRow > dstRow) {
      for (int i=0; i<count; ++i) {
	Node *n = parent->takeChild(srcRow+i); 
	parent->insertChild(dstRow+i, n);
      }
    }
    else {
      Q_ASSERT(srcRow < dstRow);
      for (int i=0; i<count; ++i) {
	Node *n = parent->takeChild(srcRow); 
	parent->insertChild(dstRow-1, n);	
      }
    }
    
  }
  else {
    //different parents

    Node *dstParent = nodeFromModelIndex(dstParentIndex);
    Q_ASSERT(dstParent);
    Q_ASSERT(dstParent != srcParent);
  
    for (int i=0; i<count; ++i) {
       Node *n = srcParent->takeChild(srcRow);
       dstParent->insertChild(dstRow+i, n);
    }

  }

  endMoveRows();

  return true;
}

bool
DeviceExplorerModel::undoMoveRows(const QModelIndex &srcParentIndex, int srcRow, int count, 
			       const QModelIndex &dstParentIndex, int dstRow)
{
  /*
    Here parameters are passed in the same order (and with the same names)
     than in moveRows(). 
     That is we compute the undo operation of a moveRows called with these parameters.
     Thus src & dst should be interpreted as source & destination for the original move operation.
  */
     
  if (!srcParentIndex.isValid() || !dstParentIndex.isValid())
    return false;
  if (srcParentIndex == dstParentIndex && (srcRow <= dstRow && dstRow <= srcRow+count-1) )
    return false;
  
  Node *srcParent = nodeFromModelIndex(srcParentIndex);
  Q_ASSERT(srcParent);

  /*
  if (srcRow+count > srcParent->childCount())
    return false;
  */

  if (srcParentIndex == dstParentIndex) {
    //move up or down inside the same parent

    Node *parent = srcParent;
    Q_ASSERT(parent);
    
    if (srcRow > dstRow) {

      beginMoveRows(dstParentIndex, dstRow, dstRow+count-1, srcParentIndex, srcRow+count);

      for (int i=0; i<count; ++i) {
	Node *n = parent->takeChild(dstRow); 
	parent->insertChild(srcRow+count-1, n);
      }
    }
    else {
      Q_ASSERT(srcRow < dstRow);

      beginMoveRows(dstParentIndex, dstRow-count, dstRow-count+count-1, srcParentIndex, srcRow);

      for (int i=0; i<count; ++i) {
	
	Node *n = parent->takeChild(dstRow-count+i); 
	parent->insertChild(srcRow+i, n);
      }
    }
    
  }
  else {
    //different parents

    beginMoveRows(dstParentIndex, dstRow, dstRow+count-1, srcParentIndex, srcRow);

    Node *dstParent = nodeFromModelIndex(dstParentIndex);
    Q_ASSERT(dstParent);
    Q_ASSERT(dstParent != srcParent);
  
    for (int i=0; i<count; ++i) {
       Node *n = dstParent->takeChild(dstRow);
       srcParent->insertChild(srcRow+i, n);
    }

  }

  endMoveRows();

  return true;  
}



QModelIndex
DeviceExplorerModel::moveUp(const QModelIndex &index)
{
  if (!index.isValid() || index.row() <= 0)
    return index;
  
  Node *n = nodeFromModelIndex(index);
  Q_ASSERT(n);
  Node *parent = n->parent();
  Q_ASSERT(parent);
  
  const int oldRow = index.row();
  const int newRow = oldRow-1;

  Node *grandparent = parent->parent();
  Q_ASSERT(grandparent);
  int rowParent = grandparent->indexOfChild(parent);
  QModelIndex srcParentIndex = createIndex(rowParent, 0, parent);

#if 0
  beginMoveRows(srcParentIndex, oldRow, oldRow, srcParentIndex, newRow);

  //parent->swapChildren(oldRow, newRow);
  {
    parent->takeChild(oldRow);
    parent->insertChild(newRow, n); 
  }
  
  endMoveRows();

  return createIndex(newRow, 0, n);

#else

#if 0
  bool moved = moveRow(srcParentIndex, oldRow, srcParentIndex, newRow);
  if (moved)
    return createIndex(newRow, 0, n);
  else
    return index;
#else

  DeviceExplorerMoveCommand *cmd = new DeviceExplorerMoveCommand;
  cmd->set(srcParentIndex, oldRow, 1, srcParentIndex, newRow, tr("Move up %1").arg(n->name()) , this);
  Q_ASSERT(m_cmdQ);
  m_cmdQ->push(cmd);
  if (! m_cachedResult) {
    delete m_cmdQ->command(0);
    m_cachedResult = true;
    return index;
  }
  return createIndex(newRow, 0, n);

#endif


#endif


}

QModelIndex
DeviceExplorerModel::moveDown(const QModelIndex &index)
{
  if (!index.isValid())
    return index;
  
  Node *n = nodeFromModelIndex(index);
  Q_ASSERT(n);
  Node *parent = n->parent();
  Q_ASSERT(parent);
  
  int oldRow = index.row();
  Q_ASSERT(parent->indexOfChild(n) == oldRow);
  int newRow = oldRow+1;

  if (newRow >= parent->childCount())
    return index;

  Node *grandparent = parent->parent();
  Q_ASSERT(grandparent);
  int rowParent = grandparent->indexOfChild(parent);
  QModelIndex srcParentIndex = createIndex(rowParent, 0, parent);

#if 0

  beginMoveRows(srcParentIndex, oldRow, oldRow, srcParentIndex, newRow+1); //+1 because moved before, cf doc.

  //parent->swapChildren(oldRow, newRow);
  {
    Node *n2 = parent->takeChild(oldRow);
    Q_ASSERT(n2 == n);
    parent->insertChild(newRow, n); 
  }

  endMoveRows();

  return createIndex(newRow, 0, n);
#else
  
#if 0
  const bool moved = moveRow(srcParentIndex, oldRow, srcParentIndex, newRow+1); //newRow+1 because moved before, cf doc.
  if (moved)
    return createIndex(newRow, 0, n);
  else
    return index;
#else

  DeviceExplorerMoveCommand *cmd = new DeviceExplorerMoveCommand;
  cmd->set(srcParentIndex, oldRow, 1, srcParentIndex, newRow+1, tr("Move down %1").arg(n->name()) , this);
  //newRow+1 because moved before, cf doc.
  Q_ASSERT(m_cmdQ);
  m_cmdQ->push(cmd);
  if (! m_cachedResult) {
    delete m_cmdQ->command(0);
    m_cachedResult = true;
    return index;
  }
  return createIndex(newRow, 0, n);  

#endif

#endif

}


QModelIndex
DeviceExplorerModel::promote(const QModelIndex &index) //== moveLeft
{
  if (!index.isValid())
    return index;
  
  Node *n = nodeFromModelIndex(index);
  Q_ASSERT(n);
  Node *parent = n->parent();
  Q_ASSERT(parent);
  
  if (parent == m_rootNode)
    return index; // Already a top-level item

  Node *grandParent = parent->parent();
  Q_ASSERT(grandParent);
  if (grandParent == m_rootNode)
    return index; //We cannot move an Address at Device level.
  
  Node *grandGrandParent = grandParent->parent();
  

  int row = parent->indexOfChild(n);
  int rowParent = grandParent->indexOfChild(parent);
  int rowGrandParent = grandGrandParent->indexOfChild(grandParent);

  QModelIndex srcParentIndex = createIndex(rowParent, 0, parent);
  QModelIndex dstParentIndex = createIndex(rowGrandParent, 0 , grandParent);

#if 0

  beginMoveRows(srcParentIndex, row, row, dstParentIndex, rowParent+1); //+1 because moved before, cf doc.
  
  Node *child = parent->takeChild(row);
  Q_ASSERT(child == n);

  grandParent->insertChild(rowParent+1, child);

  endMoveRows();

  return createIndex(rowParent+1, 0, n); 

#else

#if 0
  const bool moved = moveRow(srcParentIndex, row, dstParentIndex, rowParent+1);
  if (moved)
    return createIndex(rowParent+1, 0, n);
  else
    return index;
#else

  DeviceExplorerMoveCommand *cmd = new DeviceExplorerMoveCommand;
  cmd->set(srcParentIndex, row, 1, dstParentIndex, rowParent+1, tr("Promote %1").arg(n->name()) , this);
  Q_ASSERT(m_cmdQ);
  m_cmdQ->push(cmd);
  if (! m_cachedResult) {
    delete m_cmdQ->command(0);
    m_cachedResult = true;
    return index;
  }
  return createIndex(rowParent+1, 0, n);  

#endif


#endif

}

QModelIndex
DeviceExplorerModel::demote(const QModelIndex &index) //== moveRight
{
  if (!index.isValid())
    return index;
  
  Node *n = nodeFromModelIndex(index);
  Q_ASSERT(n);
  Node *parent = n->parent();
  Q_ASSERT(parent);
  
  if (parent == m_rootNode)
    return index; //we can not demote/moveRight device nodes
  
  int row = parent->indexOfChild(n);
  if (row == 0)
    return index; // No preceding sibling to move this under
  
  Node *sibling = parent->childAt(row-1);
  Q_ASSERT(sibling);

  Node *grandParent = parent->parent();
  int rowParent = grandParent->indexOfChild(parent);

  QModelIndex srcParentIndex = createIndex(rowParent, 0, parent);
  QModelIndex dstParentIndex = createIndex(row-1, 0, sibling);

#if 0

  beginMoveRows(srcParentIndex, row, row, dstParentIndex, sibling->childCount()); 

  Node *child = parent->takeChild(row);
  Q_ASSERT(child == n);

  //sibling->addChild(child);
  sibling->insertChild(sibling->childCount(), child);

  endMoveRows();

  return createIndex(sibling->childCount()-1, 0, n);

#else

#if 0
  const bool moved = moveRow(srcParentIndex, row, dstParentIndex, sibling->childCount());
  if (moved)
    return createIndex(sibling->childCount()-1, 0, n);
  else
    return index;
#else

  DeviceExplorerMoveCommand *cmd = new DeviceExplorerMoveCommand;
  cmd->set(srcParentIndex, row, 1, dstParentIndex, sibling->childCount(), tr("Demote %1").arg(n->name()) , this);
  Q_ASSERT(m_cmdQ);
  m_cmdQ->push(cmd);
  if (! m_cachedResult) {
    delete m_cmdQ->command(0);
    m_cachedResult = true;
    return index;
  }
  return createIndex(sibling->childCount()-1, 0, n);

#endif

#endif

}

namespace {
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
QMimeData *
DeviceExplorerModel::mimeData(const QModelIndexList &indexes) const
{
  Q_ASSERT(indexes.count());

  //we drag only one node (and its children recursively).
  if (indexes.count() != 1) 
    return nullptr;

  const QModelIndex index = indexes.at(0);

  Node *n = nodeFromModelIndex(index);
  if (n) {

    Q_ASSERT(n != m_rootNode); //m_rootNode not displayed thus should not be draggable

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

    QMimeData *mimeData = new QMimeData;

    if (n->isDevice())
      mimeData->setData(MimeTypeDevice, dataC);
    else
      mimeData->setData(MimeTypeAddress, dataC);
    
    return mimeData;
  }
  
  return nullptr;
}


bool
DeviceExplorerModel::canDropMimeData(const QMimeData *mimeData, 
				  Qt::DropAction action, 
				  int /*row*/, int /*column*/, const QModelIndex &parent) const
{
  if (action == Qt::IgnoreAction)
    return true;
  
  if (action != Qt::MoveAction && action != Qt::CopyAction) {
    std::cerr<<"dropMimeData(): not Qt::MoveAction or Qt::CopyAction ! NOT DONE \n";
    return false;
  }
  
  if (! mimeData || (! mimeData->hasFormat(MimeTypeDevice) && ! mimeData->hasFormat(MimeTypeAddress)) )
    return false;
  
  
  Node *parentNode = nodeFromModelIndex(parent);
  if (mimeData->hasFormat(MimeTypeAddress)) {
    if (parentNode == m_rootNode) {
      return false;
    }
  }
  else {
    Q_ASSERT(mimeData->hasFormat(MimeTypeDevice));
    if (parentNode != m_rootNode) {
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
DeviceExplorerModel::dropMimeData(const QMimeData *mimeData,
			       Qt::DropAction action,
			       int row, int column, const QModelIndex &parent)
{
  if (action == Qt::IgnoreAction)
    return true;

  if (action != Qt::MoveAction && action != Qt::CopyAction) {
    std::cerr<<"dropMimeData(): not Qt::MoveAction or Qt::CopyAction ! NOT DONE \n";
    return false;
  }
  
  if (! mimeData || (! mimeData->hasFormat(MimeTypeDevice) && ! mimeData->hasFormat(MimeTypeAddress)) )
    return false;

  /*
  if (column > 0) {
    std::cerr<<"dropMimeData(): tried to drop on column="<<column<<" ! NOT DONE\n";
    return false;
  }
  */

  if (column != NAME_COLUMN)
    column = NAME_COLUMN;

  QModelIndex parentIndex; //invalid
  Node *parentNode = m_rootNode;
  QString mimeType = MimeTypeDevice;
  if (mimeData->hasFormat(MimeTypeAddress)) {
    parentIndex = parent;
    parentNode = nodeFromModelIndex(parent);
    mimeType = MimeTypeAddress;

    if (parentNode == m_rootNode) {
      std::cerr<<"dropMimeData(): cannot drop an address at device level \n";
      return false;
    }

  }
  else {
    Q_ASSERT(mimeData->hasFormat(MimeTypeDevice));
    Q_ASSERT(mimeType == MimeTypeDevice);
  }

  if (parentNode) {

    if (row == -1)
      row = parentNode->childCount(); //parent.isValid() ? parent.row() : parentNode->childCount();
    
#if 0
    /*
    QByteArray data = qUncompress(mimeData->data(mimeType));
    QDataStream stream(&data, QIODevice::ReadOnly);
    BinaryReader br(stream);

    beginInsertRows(parentIndex, row, row);

    br.read(parentNode);
    
    endInsertRows();
    */
    const bool insertOk = insertTreeData(parentIndex, row, mimeData->data(mimeType));
    Q_ASSERT(insertOk);

    return true;
#else
    DeviceExplorerInsertCommand *cmd = new DeviceExplorerInsertCommand;
    const QString actionStr = (action == Qt::MoveAction ? tr("move") : tr("copy"));
    cmd->set(parentIndex, row, mimeData->data(mimeType), tr("Drop (%1)").arg(actionStr), this);
    Q_ASSERT(m_cmdQ);
    m_cmdQ->push(cmd);
    if (! m_cachedResult) {
      delete m_cmdQ->command(0);
      m_cachedResult = true;
      return false;
    }
    return true;
#endif

  }

  return false;
}


bool
DeviceExplorerModel::getTreeData(const QModelIndex &index, QByteArray &dataC) const
{
  if (! index.isValid()) 
    return false;

  Node *n = nodeFromModelIndex(index);
  if (n) {
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
DeviceExplorerModel::insertTreeData(const QModelIndex &parent, int row, const QByteArray &dataC)
{
  Node *parentNode = nodeFromModelIndex(parent);
  if (parentNode) {
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
  m_cachedResult = r;
}

DeviceExplorerModel::Path
DeviceExplorerModel::pathFromIndex(const QModelIndex &index)
{
  Path path;
  QModelIndex iter = index;
  while (iter.isValid()) {
    path.prepend(iter.row());
    iter = iter.parent();
  }
  return path;
}

QModelIndex
DeviceExplorerModel::pathToIndex(const DeviceExplorerModel::Path &path)
{
  QModelIndex iter;
  const int pathSize = path.size();
  for (int i=0; i<pathSize; ++i) {
    iter = index(path[i], 0, iter);
  }
  return iter;
}

void
DeviceExplorerModel::serializePath(QDataStream &d, const DeviceExplorerModel::Path &p)
{
  const int size = p.size();
  d << (qint32)size;
  for (int i=0; i<size; ++i)
    d << (qint32)p.at(i);
}

void
DeviceExplorerModel::deserializePath(QDataStream &d, DeviceExplorerModel::Path &p)
{
  qint32 size;
  d >> size;
  p.reserve(size);
  for (int i=0; i<size; ++i) {
    qint32 v;
    d >> v;
    p.append(v);
  }
}


void
DeviceExplorerModel::debug_printPath(const DeviceExplorerModel::Path &path)
{
  const int pathSize = path.size();
  for (int i=0; i<pathSize; ++i) {
    std::cerr<<path[i]<<" ";
  }
  std::cerr<<"\n";
}

void
DeviceExplorerModel::debug_printIndexes(const QModelIndexList &indexes)
{
  std::cerr<<"indexes: "<<indexes.size()<<" nodes: \n";
  foreach (const QModelIndex &index, indexes) {
    if (index.isValid()) {
      std::cerr<<" index.row="<<index.row()<<" col="<<index.column()<<" ";
      Node *n = nodeFromModelIndex(index);
      if (n) {
	std::cerr<<" n="<<n<<" ";
	Node *parent = n->parent();
	if (n == m_rootNode) {
	  std::cerr<<" rootNode parent="<<parent<<"\n";
	}
	else {
	  std::cerr<<" n->name="<<n->name().toStdString();
	  std::cerr<<" parent="<<parent;
	  std::cerr<<" parent->name="<<parent->name().toStdString()<<"\n";
	}
      }
      else {
	std::cerr<<" invalid node\n";
      } 
    }
    else {
	std::cerr<<" invalid index \n";
    }
  }
}
