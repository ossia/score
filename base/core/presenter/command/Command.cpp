#include "Command.hpp"
#include <QApplication>
#include <typeinfo>
#include <QDebug>

using namespace iscore;

QByteArray Command::serialize()
{
	QByteArray arr;
	{
		QDataStream s(&arr, QIODevice::Append);
		s.setVersion(QDataStream::Qt_5_3);

		s << timestamp();
	}

	return arr;
}

void Command::cmd_deserialize(QBuffer* buf)
{
	QDataStream s(buf);
	s.setVersion(QDataStream::Qt_5_3);

	int stmp;
	s >> stmp;

	m_timestamp = std::chrono::duration<quint32>(stmp);
}
