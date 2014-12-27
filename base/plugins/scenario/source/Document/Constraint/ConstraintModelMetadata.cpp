#include "ConstraintModelMetadata.hpp"


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