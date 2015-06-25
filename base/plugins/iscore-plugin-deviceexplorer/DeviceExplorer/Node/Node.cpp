#include "Node.hpp"
#include <QJsonArray>
using namespace iscore;

Node::Node(InvisibleRootNodeTag)
{
    deviceSettings().name = "___InvisibleRootNodeTag___";
}

bool Node::isInvisibleRoot() const
{
    return  "___InvisibleRootNodeTag___" == deviceSettings().name;
}

Node::Node(const AddressSettings& settings, Node* parent):
    m_parent{parent},
    m_addressSettings(settings)
{
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

Node::Node(const DeviceSettings& settings,
           Node* parent) :
    m_parent{parent},
    m_deviceSettings(settings)
{
}


Node::~Node()
{
    qDeleteAll(m_children);  //calls delete on each children
}

#include <QDebug>
QStringList Node::fullPathWithDevice() const
{
    QStringList l;
    const Node* n = this;
    while(n->parent() && !n->isDevice())
    {
        l.append(n->addressSettings().name);
        n = n->parent();
    }

    Q_ASSERT(n->isDevice());
    l.append(n->deviceSettings().name);

    std::reverse(l.begin(), l.end());
    return l;
}

QStringList Node::fullPathWithoutDevice() const
{
    if(isDevice())
    {
        return {};
    }

    QStringList l;
    const Node* n = this;
    while(n->parent() && !n->isDevice())
    {
        l.append(n->addressSettings().name);
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
QString Node::displayName() const
{
    return isDevice()? m_deviceSettings.name : m_addressSettings.name;
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
    return m_addressSettings.ioType == IOType::InOut ||  m_addressSettings.ioType == IOType::Invalid;
}

bool Node::isDevice() const
{
    return deviceSettings().protocol != "";
}

void Node::setDeviceSettings(const DeviceSettings &settings)
{
    m_deviceSettings = settings;
}

const DeviceSettings& Node::deviceSettings() const
{
    return m_deviceSettings;
}
DeviceSettings& Node::deviceSettings()
{
    return m_deviceSettings;
}

void Node::setAddressSettings(const AddressSettings &settings)
{
    m_addressSettings = settings;
    return;
}

const AddressSettings &Node::addressSettings() const
{
    return m_addressSettings;
}

AddressSettings&Node::addressSettings()
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

namespace iscore
{
Node* getNodeFromString(Node* n, QStringList&& parts)
{
    if(parts.size() == 0)
        return n;

    for(auto child : n->children())
    {
        if(child->displayName() == parts[0])
        {
            parts.removeFirst();
            return getNodeFromString(child, std::move(parts));
        }
    }

    Q_ASSERT(false);
    return nullptr;
}
}

