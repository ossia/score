#include "Node.hpp"
#include <QJsonArray>
QString Node::INVALID_STR = "-_-";
float Node::INVALID_FLOAT = std::numeric_limits<float>::max();


Node::Node(const QString& name, Node* parent)
	: m_name(name),
	  m_ioType(Invalid),
	  m_min(INVALID_FLOAT),
	  m_max(INVALID_FLOAT),
	  m_parent(parent)
{
	if (m_parent)
		m_parent->addChild(this);
}

Node::~Node()
{
	qDeleteAll(m_children); //calls delete on each children
}

Node*Node::parent() const { return m_parent; }

Node*Node::childAt(int index) const { return m_children.value(index); }

int Node::indexOfChild(Node* child) const { return m_children.indexOf(child); }

int Node::childCount() const { return m_children.count(); }

bool Node::hasChildren() const { return ! m_children.empty(); }

QList<Node*> Node::children() const { return m_children; }

void Node::insertChild(int index, Node* n)
{
	Q_ASSERT(n);
	n->m_parent = this;
	m_children.insert(index, n);

	//TODO: change corresponding API::Address ?
}

void Node::addChild(Node* n)
{
	Q_ASSERT(n);
	n->m_parent = this;
	m_children.append(n);

	//TODO: change corresponding API::Address ?
}

void Node::swapChildren(int oldIndex, int newIndex)
{
	Q_ASSERT(oldIndex < m_children.count());
	Q_ASSERT(newIndex < m_children.count());

	m_children.swap(oldIndex, newIndex);
}

Node*Node::takeChild(int index)
{
	Node *n = m_children.takeAt(index);
	Q_ASSERT(n);
	n->m_parent = 0;
	return n;
}

QString Node::name() const { return m_name; }

QString Node::value() const { return m_value; }

Node::IOType Node::ioType() const { return m_ioType; }

float Node::minValue() const { return m_min; }

float Node::maxValue() const { return m_max; }

unsigned int Node::priority() const { return m_priority; }

void Node::setName(const QString& name) { m_name = name; }

void Node::setValue(const QString& value) { m_value = value; }

void Node::setIOType(const Node::IOType ioType) { m_ioType = ioType; }

void Node::setMinValue(float minV) { m_min = minV; }

void Node::setMaxValue(float maxV) { m_max = maxV; }

void Node::setPriority(unsigned int priority) { m_priority = priority; }

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
	if (parent()) {
		return parent()->parent() == nullptr;
	}
	return false;
}

Node*Node::clone() const
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


QJsonObject nodeToJson(const Node* n)
{
	QJsonObject obj;
	obj["Name"] = n->name();
	obj["Value"] = n->value();
	obj["IOType"] = n->ioType();
	obj["MinValue"] = n->minValue();
	obj["MaxValue"] = n->maxValue();
	obj["Priority"] = (int)n->priority();

	QJsonArray arr;
	for(const Node* child : n->children())
	{
		arr.append(nodeToJson(child));
	}

	obj["Children"] = arr;

	return obj;
}
