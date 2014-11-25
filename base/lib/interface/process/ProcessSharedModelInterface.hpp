#pragma once
#include <QNamedObject>

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
			virtual ProcessViewModelInterface* makeViewModel(int id, QObject* parent) = 0;
	};

}
