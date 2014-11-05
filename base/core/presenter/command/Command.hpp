#pragma once
#include <QUndoCommand>
#include <QByteArray>
#include <QDataStream>
#include <QBuffer>
#include <chrono>

namespace iscore
{
	/**
	 * @brief The Command class
	 * 
	 * The base of the command system in i-score
	 * It is timestamped, because we can then compare between clients.
	 * 
	 * Maybe the NetworkPlugin should replace the Command by a TimestampedCommand instead ?
	 * What if other plug-ins also want to add functionality ? 
	 */
	class Command : public QUndoCommand
	{
			enum class Origin { Local, Remote };
		public:
			Command(QString parname, QString cmdname, QString text, Origin orig):
				QUndoCommand{text},
				m_name{cmdname},
				m_parentName{parname},
				m_origin{orig}
			{
			}

			Command(QString parname,
					QString cmdname,
					QString text):
				Command(parname, cmdname, text, Origin::Local)
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
			const Origin m_origin;
			
			//TODO check if this is UTC
            std::chrono::milliseconds m_timestamp
			{
                std::chrono::duration_cast<std::chrono::milliseconds>
				(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
				)
			};
	};
}
