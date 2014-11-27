#pragma once
#include <QNamedObject>

namespace iscore
{
	/**
	 * @brief The ProcessViewModelInterface class
	 *
	 * Interface to implement to make a process view model.  
	 */
	class ProcessViewModelInterface: public QIdentifiedObject
	{
		public:
			ProcessViewModelInterface(QObject* parent, QString name, int viewModelId, int sharedProcessId): 
				QIdentifiedObject{parent, name, viewModelId},
				m_sharedProcessId{sharedProcessId}
			{
				
			}
			
			virtual ~ProcessViewModelInterface() = default;
			
			int sharedProcessId() const
			{ return m_sharedProcessId; }
			
			virtual void serialize(QDataStream&) const = 0;
			virtual void deserialize(QDataStream&) = 0;
			
		private:
			int m_sharedProcessId{};
	};

}


inline QDataStream& operator <<(QDataStream& s, const iscore::ProcessViewModelInterface& p)
{
	s << p.id() 
	  << p.objectName()
	  << p.sharedProcessId();
	
	p.serialize(s);
	return s;
}

