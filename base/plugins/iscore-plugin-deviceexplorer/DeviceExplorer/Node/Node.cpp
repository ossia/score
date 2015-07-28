#include "Node.hpp"

namespace iscore
{

Node::Node():
    m_rootNode{true},
    m_type{Type::RootNode}
{

}

Node::Node(InvisibleRootNodeTag):
    m_rootNode{true},
    m_type{Type::RootNode}
{
}

bool Node::isInvisibleRoot() const
{
    return m_type == Type::RootNode;
}

Node::Node(const iscore::AddressSettings& settings,
           Node* parent):
    m_parent{parent},
    m_addressSettings(settings),
    m_type{Type::Address}
{
    if(m_parent)
    {
        m_parent->addChild(this);
    }
}

Node::Node(const Node &source, Node *parent):
    m_parent{parent},
    m_rootNode{false},
    m_type{source.type()}
{
    if(source.isDevice())
        setDeviceSettings(source.deviceSettings());
    else
        setAddressSettings(source.addressSettings());

    for(const auto& child : source.children())
    {
        this->addChild(new Node{*child, this});
    }
}

Node& Node::operator=(const Node &source)
{
    m_type = source.m_type;

    if(source.isDevice())
        setDeviceSettings(source.deviceSettings());
    else
        setAddressSettings(source.addressSettings());

    for(const auto& child : source.children())
    {
        this->addChild(new Node{*child, this});
    }

    return *this;
}

Node::Node(const DeviceSettings& settings,
           Node* parent) :
    m_parent{parent},
    m_deviceSettings(settings),
    m_type{Type::Device}
{
}


Node::~Node()
{
    qDeleteAll(m_children);  //calls delete on each children
}

iscore::Address Node::address() const
{
    Address addr;
    const Node* n = this;
    while(n->parent() && !n->isDevice())
    {
        addr.path.prepend(n->addressSettings().name);
        n = n->parent();
    }

    Q_ASSERT(n->isDevice());
    addr.device = n->deviceSettings().name;

    return addr;
}

Node::Type Node::type() const
{
    return m_type;
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

int Node::indexOfChild(const Node* child) const
{
    return m_children.indexOf(const_cast<Node*>(child));
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
QString Node::displayName() const
{
    switch(m_type)
    {
        case Type::Address:
            return m_addressSettings.name;
            break;
        case Type::Device:
            return m_deviceSettings.name;
            break;
        case Type::RootNode:
            return "Invisible Root Node";
            break;
    }
}

/* *************************************************************
 * COLUMNS MODIFIERS
 * ************************************************************/


// ******************************************************************

bool Node::isSelectable() const
{
    return true;
}

bool Node::isEditable() const
{
    return m_type == Type::Address
            && (m_addressSettings.ioType == IOType::InOut
               ||  m_addressSettings.ioType == IOType::Invalid);
}

bool Node::isDevice() const
{
    return m_type == Type::Device;
}

void Node::setDeviceSettings(const DeviceSettings &settings)
{
    if(m_type == Type::Address)
        m_addressSettings.~AddressSettings();

    new (&m_deviceSettings) DeviceSettings(settings);
    m_type = Type::Device;
}

const DeviceSettings& Node::deviceSettings() const
{
    return m_deviceSettings;
}

DeviceSettings& Node::deviceSettings()
{
    return m_deviceSettings;
}

void Node::setAddressSettings(const iscore::AddressSettings &settings)
{
    if(m_type == Type::Device)
        m_deviceSettings.~DeviceSettings();

    new (&m_addressSettings) AddressSettings(settings);
    m_type = Type::Address;
}

const iscore::AddressSettings &Node::addressSettings() const
{
    return m_addressSettings;
}

iscore::AddressSettings &Node::addressSettings()
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

Node* getNodeFromString(Node* n, QStringList&& parts)
{
    auto theN = try_getNodeFromString(n, std::move(parts));
    Q_ASSERT(theN);
    return theN;
}

Node* try_getNodeFromString(Node* n, QStringList&& parts)
{
    if(parts.size() == 0)
        return n;

    for(auto child : n->children())
    {
        if(child->displayName() == parts[0])
        {
            parts.removeFirst();
            return try_getNodeFromString(child, std::move(parts));
        }
    }

    return nullptr;
}
}

