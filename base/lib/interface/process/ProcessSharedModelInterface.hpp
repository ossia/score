#pragma once
#include <tools/IdentifiedObject.hpp>
class QDataStream;
namespace iscore
{
	class ProcessViewModelInterface;
	/**
	 * @brief The ProcessSharedModelInterface class
	 *
	 * Interface to implement to make a process.
	 */
	class ProcessSharedModelInterface: public IdentifiedObject
	{
		public:
			using IdentifiedObject::IdentifiedObject;

			/**
			 * @brief processName
			 * @return the name of the process.
			 *
			 * Needed for serialization - deserialization, in order to recreate
			 * a new process from the same plug-in.
			 */
			virtual QString processName() const = 0; // Needed for serialization.

			virtual ~ProcessSharedModelInterface() = default;
			virtual ProcessViewModelInterface* makeViewModel(int viewModelId,
															 int sharedProcessId,
															 QObject* parent) = 0;
			virtual ProcessViewModelInterface* makeViewModel(QDataStream& s,
															 QObject* parent) = 0;

			virtual void serialize(QDataStream&) const = 0;
			virtual void deserialize(QDataStream&) = 0;
	};
}


#include <QDebug>

inline QDataStream& operator <<(QDataStream& s, const iscore::ProcessSharedModelInterface& p)
{
	qDebug(Q_FUNC_INFO);
	s << p.id();
	s << p.objectName();

	qDebug() << p.id() << p.objectName();
	p.serialize(s);
	return s;
}

inline QDataStream& operator >>(QDataStream& s, iscore::ProcessSharedModelInterface& p)
{
	qDebug(Q_FUNC_INFO);
	int id;
	QString name;
	s >> id >> name;
	p.setId(id);
	p.setObjectName(name);

	qDebug() << id << name;
	p.deserialize(s);
	return s;
}
