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
			
			virtual ~ProcessSharedModelInterface() = default;
			virtual ProcessViewModelInterface* makeViewModel(int viewModelId, int sharedProcessId, QObject* parent) = 0;
			
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
