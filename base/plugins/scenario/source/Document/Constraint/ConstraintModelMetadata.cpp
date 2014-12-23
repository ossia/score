#include "ConstraintModelMetadata.hpp"

QDataStream& operator<<(QDataStream& s, const ConstraintModelMetadata& m)
{
	s << m.name() << m.comment() << m.color();

	return s;
}

QDataStream& operator>>(QDataStream& s, ConstraintModelMetadata& m)
{
	QString name, comment;
	QColor color;
	s >> name >> comment >> color;

	m.setName(name);
	m.setComment(comment);
	m.setColor(color);

	return s;
}

//// Simple properties
QString ConstraintModelMetadata::name() const
{
	return m_name;
}

QString ConstraintModelMetadata::comment() const
{
	return m_comment;
}

QColor ConstraintModelMetadata::color() const
{
	return m_color;
}




void ConstraintModelMetadata::setName(QString arg)
{
	if (m_name == arg)
		return;

	m_name = arg;
	emit nameChanged(arg);
}

void ConstraintModelMetadata::setComment(QString arg)
{
	if (m_comment == arg)
		return;

	m_comment = arg;
	emit commentChanged(arg);
}

void ConstraintModelMetadata::setColor(QColor arg)
{
	if (m_color == arg)
		return;

	m_color = arg;
	emit colorChanged(arg);
}