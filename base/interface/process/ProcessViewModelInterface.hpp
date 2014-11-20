#pragma once
#include <QObject>

namespace iscore
{
	/**
	 * @brief The ProcessViewModelInterface class
	 *
	 * Interface to implement to make a process view model.  
	 */
	class ProcessViewModelInterface: public QObject
	{
		public:
			ProcessViewModelInterface(unsigned int id, QObject* parent):
				QObject{parent},
				m_modelId{id}
			{
				
			}
			
			virtual ~ProcessViewModelInterface() = default;
			
		private:
			const unsigned int m_modelId;
	};

}
