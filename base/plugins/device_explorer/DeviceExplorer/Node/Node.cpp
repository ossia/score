#include "Node.hpp"
#include <QJsonArray>
// TODO replace INVALID_stuff with boost::optional values.
QString Node::INVALID_STR = "-_-";
float Node::INVALID_FLOAT = std::numeric_limits<float>::max();


Node::Node(const QString& name, Node* parent)
    : m_name(name),
      m_ioType(Invalid),
      m_min(INVALID_FLOAT),
      m_max(INVALID_FLOAT),
      m_parent(parent)
{
    if(m_parent)
    {
        m_parent->addChild(this);
    }
}

Node::Node(const DeviceSettings& devices,
           const QString& name,
           Node* parent) :
    Node {name, parent}
{
    m_deviceSettings = devices;
}


Node::~Node()
{
    qDeleteAll(m_children);  //calls delete on each children
}

void Node::setParent(Node* parent)
{
    if(m_parent)
        m_parent->removeChild(this);

    m_parent = parent;
    m_parent->addChild(this);
}

Node* Node::parent() const
{
    return m_parent;
}

Node* Node::childAt(int index) const
{
    return m_children.value(index);
}

int Node::indexOfChild(Node* child) const
{
    return m_children.indexOf(child);
}

int Node::childCount() const
{
    return m_children.count();
}

bool Node::hasChildren() const
{
    return ! m_children.empty();
}

QList<Node*> Node::children() const
{
    return m_children;
}

void Node::insertChild(int index, Node* n)
{
    Q_ASSERT(n);
    n->m_parent = this;
    m_children.insert(index, n);
}

void Node::addChild(Node* n)
{
    Q_ASSERT(n);
    n->m_parent = this;
    m_children.append(n);
}

void Node::swapChildren(int oldIndex, int newIndex)
{
    Q_ASSERT(oldIndex < m_children.count());
    Q_ASSERT(newIndex < m_children.count());

    m_children.swap(oldIndex, newIndex);
}

Node* Node::takeChild(int index)
{
    Node* n = m_children.takeAt(index);
    Q_ASSERT(n);
    n->m_parent = 0;
    return n;
}

void Node::removeChild(Node* child)
{
    m_children.removeAll(child);
}

QString Node::name() const
{
    return m_name;
}

QString Node::value() const
{
    return m_value;
}

Node::IOType Node::ioType() const
{
    return m_ioType;
}

float Node::minValue() const
{
    return m_min;
}

float Node::maxValue() const
{
    return m_max;
}

unsigned int Node::priority() const
{
    return m_priority;
}

void Node::setName(const QString& name)
{
    m_name = name;
    if (isDevice())
    {
        m_deviceSettings.name = name;
    }
}

void Node::setValue(const QString& value)
{
    m_value = value;
}

void Node::setIOType(const Node::IOType ioType)
{
    m_ioType = ioType;
}

void Node::setMinValue(float minV)
{
    m_min = minV;
}

void Node::setMaxValue(float maxV)
{
    m_max = maxV;
}

void Node::setPriority(unsigned int priority)
{
    m_priority = priority;
}

bool Node::isSelectable() const
{
    return true;
    //return m_ioType != Node::In;
}

bool Node::isEditable() const
{
    return m_ioType == Node::Out || m_ioType == Node::Invalid;
}

bool Node::isDevice() const
{
    if(parent())
    {
        return parent()->parent() == nullptr;
    }

    return false;
}

void Node::setDeviceSettings(DeviceSettings &settings)
{
    m_deviceSettings = settings;
    setName(settings.name);
}

const DeviceSettings& Node::deviceSettings() const
{
    return m_deviceSettings;
}

const QList<QString> Node::addressSettings() const
{
    QList<QString> list;
    list.push_back(m_name);
    list.push_back(m_value);
    list.push_back(QString(ioType()));
    list.push_back(QString::number(m_min));
    list.push_back(QString::number(m_max));
    list.push_back("unit");
    list.push_back("clipmode");
    list.push_back(QString::number(m_priority));
    list.push_back("tags");

    return list;
}

Node* Node::clone() const
{
    Node* n = new Node(*this);
    const int numChildren = this->childCount();
    n->m_children.clear();
    n->m_children.reserve(numChildren);

    for(int i = 0; i < numChildren; ++i)
    {
        (this->childAt(i)->clone())->setParent(n);
        //n->childAt(i)->setParent(n);
    }

    return n;
}


QJsonObject nodeToJson(const Node* n)
{
    QJsonObject obj;

    if(!n)
    {
        return obj;
    }

    obj["Name"] = n->name();
    obj["Value"] = n->value();
    obj["IOType"] = n->ioType();
    obj["MinValue"] = n->minValue();
    obj["MaxValue"] = n->maxValue();
    obj["Priority"] = static_cast<int>(n->priority());

    if(n->isDevice())
    {
        // TODO in a device-specific way
        // obj["DeviceSettings"] = QJsonArray::fromStringList(n->deviceSettings());
    }

    QJsonArray arr;

    for(const Node* child : n->children())
    {
        arr.append(nodeToJson(child));
    }

    obj["Children"] = arr;

    return obj;
}


QDataStream& operator<<(QDataStream& s, const Node& n)
{
    s << n.name() << n.value() << n.ioType() << n.minValue() << n.maxValue() << n.priority();

    s << n.isDevice();
    if(n.isDevice())
    {
        s << n.deviceSettings();
    }

    s << n.childCount();
    for(auto& child : n.children())
    {
        if(child)
            s << *child;
    }

    return s;
}


QDataStream& operator>>(QDataStream &s, Node &n)
{
    QString name, value;
    int io;
    float min, max;
    unsigned int prio;
    bool isDev;
    int settings;
    int childCount;
    Node child;

    s >> name >> value >> io >> min >> max >> prio >> isDev;
    if (isDev)
    {
        s >> settings;
    }
    s >> childCount;
    for (int i = 0; i < childCount; ++i)
    {
        s >> child;
        n.addChild(&child);
    }

    n.setName(name);
    n.setValue(value);
    n.setIOType(static_cast<Node::IOType>(io));
    n.setMinValue(min);
    n.setMaxValue(max);
    n.setPriority(prio);

    return s;
}


#include <QDebug>
namespace
{
    void setIOType(Node* n, const QString& type)
    {
        Q_ASSERT(n);

        if(type == "In")
        {
            n->setIOType(Node::In);
        }
        else if(type == "Out")
        {
            n->setIOType(Node::Out);
        }
        else if(type == "In/Out")
        {
            n->setIOType(Node::InOut);
        }
        else
        {
            qDebug() << "Unknown I/O type: " << type;
        }
    }
}

Node* makeNode(const QList<QString>& addressSettings)
{
    Q_ASSERT(addressSettings.size() >= 2);

    QString name = addressSettings.at(0);
    QString valueType = addressSettings.at(1);

    Node* node = new Node(name, nullptr);  //build without parent otherwise appended at the end

    if(valueType == "Int")
    {
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
    else if(valueType == "Float")
    {
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
    else if(valueType == "String")
    {
        QString ioType = addressSettings.at(2);
        QString value = addressSettings.at(3);
        QString priority = addressSettings.at(4);
        QString tags = addressSettings.at(5);
        node->setValue(value);
        node->setPriority(priority.toUInt());
        setIOType(node, ioType);
    }

    return node;
}
