#pragma once
#include <QObject>

class ProcessViewModelInterface;
namespace iscore
{
	/**
	 * @brief The ProcessSharedModelInterface class
	 *
	 * Interface to implement to make a process. 
	 */
	class ProcessSharedModelInterface: public QObject
	{
		public:
			ProcessSharedModelInterface(unsigned int id, QObject* parent):
				QObject{parent},
				m_modelId{id}
			{
				
			}
			
			virtual ~ProcessSharedModelInterface() = default;
			virtual ProcessViewModelInterface* makeViewModel(int id, QObject* parent) = 0;
			
			int id() const 
			{ return m_modelId; }
			
		private:
			const unsigned int m_modelId;
	};

}
