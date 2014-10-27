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
			Command(QString parname,
					QString cmdname,
					QString text):
				QUndoCommand{text},
				m_name{cmdname},
				m_parentName{parname}
			{
			}

			QString name() const
			{ return m_name; }
			QString parentName() const
			{ return m_parentName; }

			virtual QByteArray serialize();
			virtual void deserialize(QByteArray) = 0;

		protected:
			quint32 timestamp() { return m_timestamp.count(); }
			void cmd_deserialize(QBuffer*);

		private:
			const QString m_name;
			const QString m_parentName;

			//TODO check if this is UTC
			std::chrono::milliseconds m_timestamp{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())};
	};
}
