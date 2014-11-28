#pragma once
#include <QNamedObject>
class QDataStream;
namespace iscore
{
	class ProcessViewModelInterface;
	/**
	 * @brief The ProcessSharedModelInterface class
	 *
	 * Interface to implement to make a process.
	 */
	class ProcessSharedModelInterface: public QIdentifiedObject
	{
		public:
			using QIdentifiedObject::QIdentifiedObject;

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

inline QDataStream& operator <<(QDataStream& s, const iscore::ProcessSharedModelInterface& p)
{
	s << p.id();
	s << p.objectName();

	p.serialize(s);
	return s;
}

inline QDataStream& operator >>(QDataStream& s, iscore::ProcessSharedModelInterface& p)
{
	int id;
	QString name;
	s >> id >> name;
	p.setId(id);
	p.setObjectName(name);

	p.deserialize(s);
	return s;
}
