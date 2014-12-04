#include "objectinterval.hpp"
#include <QDebug>

ObjectInterval::ObjectInterval (QObject* parent) :
	QObject (parent)
{
}

ObjectInterval::ObjectInterval (QString name, QString comments, QColor color, QObject* parent) :
	ObjectInterval (parent)
{
	_name = name;
	_comments = comments;
	_color = color;
	_automations = new std::vector<QString>;
	_automations->push_back ("automation/nb/1");
	_automations->push_back ("automation/nb/2");
	_automations->push_back ("automation/autre");
}

QString ObjectInterval::name() const
{
	return _name;
}

void ObjectInterval::setName (const QString& name)
{
	_name = name;
}
QString ObjectInterval::comments() const
{
	return _comments;
}

void ObjectInterval::setComments (const QString& comments)
{
	_comments = comments;
}
QColor ObjectInterval::color() const
{
	return _color;
}

void ObjectInterval::setColor (const QColor& color)
{
	_color = color;
}
std::vector<QString>* ObjectInterval::automations() const
{
	return _automations;
}

void ObjectInterval::setAutomations (std::vector<QString>* automations)
{
	_automations = automations;
}

