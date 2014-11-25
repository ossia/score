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
			using QIdentifiedObject::QIdentifiedObject;
			virtual ~ProcessViewModelInterface() = default;
	};

}
