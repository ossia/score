#include "Command.hpp"
#include <QApplication>
#include <typeinfo>
#include <QDebug>

using namespace iscore;

QByteArray Command::serialize()
{
	QByteArray arr;
	QDataStream s(&arr, QIODevice::Append);

	s << m_name << timestamp();
	return arr;
}

void Command::cmd_deserialize(QBuffer* buf)
{
	QDataStream s(buf);

	int stmp;
	s >> m_name >> stmp;

	m_timestamp = std::chrono::duration<quint32>(stmp);
}
