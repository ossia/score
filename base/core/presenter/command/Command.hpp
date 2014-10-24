#pragma once
#include <QUndoCommand>
#include <QByteArray>
#include <QDataStream>
#include <QBuffer>
#include <chrono>

namespace iscore
{
	// The base of the command system in i-score
	// It is timestamped, because we can then compare between clients
	class Command : public QUndoCommand
	{
		public:
			Command(QString cmdname, QString text):
				QUndoCommand{text},
				m_name{cmdname}
			{
			}

			QString name() const
			{ return m_name; }

			virtual QByteArray serialize();
			virtual void deserialize(QByteArray) = 0;

		protected:
			quint32 timestamp() { return m_timestamp.count(); }
			void cmd_deserialize(QBuffer*);


		private:
			QString m_name;
			//TODO check if this is UTC
			std::chrono::milliseconds m_timestamp{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())};
	};
}
