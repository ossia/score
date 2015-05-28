#include "Node.hpp"
#include <QJsonArray>

#include "Common/AddressSettings/AddressSpecificSettings/AddressFloatSettings.hpp"
#include "Common/AddressSettings/AddressSpecificSettings/AddressIntSettings.hpp"
#include "Common/AddressSettings/AddressSpecificSettings/AddressStringSettings.hpp"

// TODO replace INVALID_stuff with boost::optional values.
QString Node::INVALID_STR = "-_-";
float Node::INVALID_FLOAT = std::numeric_limits<float>::max();


Node::Node(const QString& name, Node* parent):
    m_parent{parent}
{
    m_addressSettings.name = name;
    if(m_parent)
    {
        m_parent->addChild(this);
    }
}

Node::Node(const Node &source, Node *parent):
    m_parent{parent}
{
    if(source.isDevice())
        m_deviceSettings = source.deviceSettings();
    m_addressSettings = source.addressSettings();

    for(const auto& child : source.children())
    {
        this->addChild(new Node{*child, this});
    }
}

Node& Node::operator=(const Node &source)
{
    if(source.isDevice())
        m_deviceSettings = source.deviceSettings();
    m_addressSettings = source.addressSettings();

    for(const auto& child : source.children())
    {
        this->addChild(new Node{*child, this});
    }

    return *this;
}

Node::Node(const DeviceSettings& devices,
           const QString& name,
           Node* parent) :
    Node {name, parent}
{
    m_addressSettings.name = name;
    m_deviceSettings = devices;
}


Node::~Node()
{
    qDeleteAll(m_children);  //calls delete on each children
}

#include <QDebug>
QStringList Node::fullPath() const
{
    QStringList l;
    const Node* n = this;
    while(n->parent())
    {
        l.append(n->name());
        n = n->parent();
    }

    std::reverse(l.begin(), l.end());
    return l;
}

void Node::setParent(Node* parent)
{
    if(m_parent)
        m_parent->removeChild(this);

    m_parent = parent;
    m_parent->addChild(this);
}

/* *************************************************************
  * ACCESSORS
  * ************************************************************/

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

/* *************************************************************
 *
 * ************************************************************/

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

/* *************************************************************
 * COLUMNS ACCESSORS
 * ************************************************************/
QString Node::name() const
{
    return m_addressSettings.name;
}

QString Node::value() const
{
    return  m_addressSettings.value.toString();
}

IOType Node::ioType() const
{
    return m_addressSettings.ioType;
}

float Node::minValue() const
{
    if(m_addressSettings.addressSpecificSettings.canConvert<AddressFloatSettings>())
    {
        return m_addressSettings.addressSpecificSettings.value<AddressFloatSettings>().min;
    }
    else if(m_addressSettings.addressSpecificSettings.canConvert<AddressIntSettings>())
    {
        return m_addressSettings.addressSpecificSettings.value<AddressIntSettings>().min;
    }
    else
    {
        // String
        return 0;
    }
}

float Node::maxValue() const
{
    if(m_addressSettings.addressSpecificSettings.canConvert<AddressFloatSettings>())
    {
        return m_addressSettings.addressSpecificSettings.value<AddressFloatSettings>().max;
    }
    else if(m_addressSettings.addressSpecificSettings.canConvert<AddressIntSettings>())
    {
        return m_addressSettings.addressSpecificSettings.value<AddressIntSettings>().max;
    }
    else
    {
        // String
        return 0;
    }
}

unsigned int Node::priority() const
{
    return m_addressSettings.priority;
}

QString Node::tags() const
{
    return m_addressSettings.tags;
}

/* *************************************************************
 * COLUMNS MODIFIERS
 * ************************************************************/

void Node::setName(const QString& name)
{
    if (isDevice())
    {
        m_deviceSettings.name = name;
    }
    else
    {
        m_addressSettings.name = name;
    }
}

void Node::setValue(const QString& value)
{
    m_addressSettings.value = QVariant::fromValue(value);
}

void Node::setValueType(const QString &value)
{
    m_addressSettings.valueType = value;
    if(value == QString("Float"))
    {
        m_addressSettings.addressSpecificSettings = QVariant::fromValue(AddressFloatSettings{});
    }
    if(value == QString("Int"))
    {
        m_addressSettings.addressSpecificSettings = QVariant::fromValue(AddressIntSettings{});
    }
    if(value == QString("String"))
    {
        // Nothing to do
    }
}

void Node::setIOType(const IOType& ioType)
{
    m_addressSettings.ioType = ioType;
}

void Node::setMinValue(float minV)
{
    if (m_addressSettings.addressSpecificSettings.canConvert<AddressFloatSettings>())
    {
        AddressFloatSettings fs = m_addressSettings.addressSpecificSettings.value<AddressFloatSettings>();
        fs.min = minV;
        m_addressSettings.addressSpecificSettings = QVariant::fromValue(fs);
    }
    if (m_addressSettings.addressSpecificSettings.canConvert<AddressIntSettings>())
    {
        AddressIntSettings is = m_addressSettings.addressSpecificSettings.value<AddressIntSettings>();
        is.min = minV;
        m_addressSettings.addressSpecificSettings = QVariant::fromValue(is);
    }
}

void Node::setMaxValue(float maxV)
{
    if (m_addressSettings.addressSpecificSettings.canConvert<AddressFloatSettings>())
    {
        AddressFloatSettings fs = m_addressSettings.addressSpecificSettings.value<AddressFloatSettings>();
        fs.max = maxV;
        m_addressSettings.addressSpecificSettings = QVariant::fromValue(fs);
    }
    if (m_addressSettings.addressSpecificSettings.canConvert<AddressIntSettings>())
    {
        AddressIntSettings is = m_addressSettings.addressSpecificSettings.value<AddressIntSettings>();
        is.max = maxV;
        m_addressSettings.addressSpecificSettings = QVariant::fromValue(is);
    }
}

void Node::setPriority(unsigned int priority)
{
    m_addressSettings.priority = priority;
}

void Node::setTags(const QString &tags)
{
    m_addressSettings.tags = tags;
}

// ******************************************************************

bool Node::isSelectable() const
{
    return true;
    //return m_ioType != Node::In;
}

bool Node::isEditable() const
{
    return m_addressSettings.ioType == IOType::InOut ||  m_addressSettings.ioType == IOType::Invalid;
}

bool Node::isDevice() const
{
    return deviceSettings().protocol != "";
}

void Node::setDeviceSettings(const DeviceSettings &settings)
{
    m_deviceSettings = settings;
    setName(settings.name);
}

const DeviceSettings& Node::deviceSettings() const
{
    return m_deviceSettings;
}

void Node::setAddressSettings(const AddressSettings &settings)
{
    setName(settings.name);
    setValueType(settings.valueType);
    setIOType(settings.ioType);
    setPriority(settings.priority);
    setTags(settings.tags);

    if(settings.addressSpecificSettings.canConvert<AddressFloatSettings>())
    {
        AddressFloatSettings fs = settings.addressSpecificSettings.value<AddressFloatSettings>();
        setMaxValue(fs.max);
        setMinValue(fs.min);

        if(m_addressSettings.addressSpecificSettings.canConvert<AddressFloatSettings>())
        {
            AddressFloatSettings mfs = m_addressSettings.addressSpecificSettings.value<AddressFloatSettings>();
            mfs.clipMode = fs.clipMode;
            mfs.unit = fs.unit;
            m_addressSettings.addressSpecificSettings = QVariant::fromValue(mfs);
        }
    }
    else if(settings.addressSpecificSettings.canConvert<AddressIntSettings>())
    {
        AddressIntSettings is = settings.addressSpecificSettings.value<AddressIntSettings>();
        setMaxValue(is.max);
        setMinValue(is.min);
        if(m_addressSettings.addressSpecificSettings.canConvert<AddressIntSettings>())
        {
            AddressIntSettings mis = m_addressSettings.addressSpecificSettings.value<AddressIntSettings>();
            mis.clipMode = is.clipMode;
            mis.unit = is.unit;
            m_addressSettings.addressSpecificSettings = QVariant::fromValue(mis);
        }
    }

    if(settings.value.canConvert<float>())
    {
        float f = settings.value.value<float>();
        setValue(QString::number(f));
    }
    else if(settings.value.canConvert<int>())
    {
        int i = settings.value.value<int>();
        setValue(QString::number(i));
    }
}

const AddressSettings &Node::addressSettings() const
{
    return m_addressSettings;
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

Node* makeNode(const AddressSettings &addressSettings)
{
    Node* node = new Node(addressSettings.name, nullptr);  //build without parent otherwise appended at the end

    node->setAddressSettings(addressSettings);

    return node;
}


Node* getNodeFromString(Node* n, QStringList&& parts)
{
    if(parts.size() == 0)
        return n;

    for(auto child : n->children())
    {
        if(child->name() == parts[0])
        {
            parts.removeFirst();
            return getNodeFromString(child, std::move(parts));
        }
    }

    Q_ASSERT(false);
    return nullptr;
}

